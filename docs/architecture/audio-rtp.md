# Phase 6 Architecture — Audio and RTP Media

## Status

**Completed** — Phase 6 milestone tag `v0.7-rtp-audio`. The media implementation, automated verification, two-way audio acceptance, and sanitized RTP study are recorded for the documented local lab. The acceptance result does not establish performance or device behavior in other environments.

## Purpose

Extend the Phase 5 PJSUA2 call lifecycle from SIP signaling with a null audio device to real Linux capture/playback and observable RTP media, without moving SIP signaling or media transport into the project's Phase 3 socket classes.

## Scope

This document describes audio-device selection, the bidirectional PJSUA2 conference-bridge wiring, media callback/lifetime handling, and read-only RTP diagnostics for the existing one-shot outgoing/incoming call CLIs. It does not specify a general audio mixer, custom RTP socket implementation, echo-cancellation policy, secure media negotiation, multi-call mixing, or a Qt user interface.

## Architecture model

### Layering and responsibility

```text
sip_call_main / sip_listen_main
  |  parse CLI options; read password from environment; print call/media results
  v
SipOutgoingCallClient / SipIncomingCallClient
  |  own pj::Endpoint, pj::Account, pj::Call and callback state
  |  select devices after Endpoint::libStart()
  +--> Pjsua2AudioDeviceManager --> AudioDeviceSelection (name/driver/capability)
  |                                |
  |                                +--> pj::AudDevManager device IDs
  |
  +--> onCallMediaState() --> Pjsua2CallAudioBridge
  |                            |
  |              capture AudioMedia --startTransmit--> call AudioMedia
  |              call AudioMedia --startTransmit--> playback AudioMedia
  |                            |
  |                    PJMEDIA conference bridge / RTP stream
  |
  +--> rtp_diagnostics() --> Pjsua2RtpDiagnostics
                              |  pj::Call::getStreamInfo/getStreamStat
                              +--> network::RtpStatistics (calculation only)
```

SIP establishes the dialog and negotiates the media parameters through SDP. PJSIP/PJMEDIA owns the call's RTP stream, codec, and conference bridge. `src/network/RtpStatistics.*` is a small arithmetic/data boundary; it does not receive packets or implement a second RTP stack. The older `src/network/UdpSocket.hpp` abstraction exercises general networking independently.

### Audio-device selection

`SipCallAudioConfig` supplies independent `AudioDeviceSelector` values for capture and playback. Both default to the device named `default`; the CLI can additionally provide a driver such as `ALSA`. The PJSUA2 adapter converts `AudDevManager::enumDev2()` results to core descriptors and resolves IDs by the selection policy and required direction. IDs are discovered at runtime, not stored as hard-coded device numbers. `configure_pjsua2_audio_devices()` calls `setCaptureDev()` and `setPlaybackDev()` with the resolved IDs.

The Phase 6 preflight (`scripts/phase6/verify-audio-environment.sh`) checks for PJPROJECT >= 2.17, compiles/links a PJSUA2 smoke program, and enumerates devices to require at least one capture-capable and one playback-capable device. That capability check does **not** prove the specific headset chosen by the user, active media routing, or audible output; live calls provide those separate observations.

### Bidirectional media connection

`active_audio_media_indices()` selects entries from `pj::CallInfo::media` whose type is audio and whose status is `PJSUA_CALL_MEDIA_ACTIVE`. `connect_active_call_audio()` connects each selected media index in both directions:

```text
microphone / capture device  -->  PJSUA2 call AudioMedia  -->  RTP send
RTP receive                 -->  PJSUA2 call AudioMedia  -->  speaker / playback device
```

Each path uses PJSUA2 `AudioMedia::startTransmit()`. The directionality is important: connecting only capture-to-call or only call-to-playback produces a one-way application audio path even when the SIP dialog and RTP stream appear healthy.

### RTP/media diagnostics

`Pjsua2RtpDiagnostics` reads every currently active audio media index with `pj::Call::getStreamInfo()` and `getStreamStat()`. It exposes the media index, codec, codec clock rate, receive/transmit payload types, transmitted packet count, receive statistics, mean receive jitter, mean RTCP round-trip time, and last transmitted RTP sequence/timestamp.

The mapping is deliberately read-only:

| CLI field | Implementation source | Interpretation |
|---|---|---|
| `[MEDIA] codec`, `clock-rate`, `rx-pt`, `tx-pt` | `pj::StreamInfo` | Negotiated stream information at snapshot time |
| `[RTP] received`, `lost` | `pj::StreamStat::rtcp.rxStat` | Receiver-side packet counters supplied by PJMEDIA |
| `[RTP] sent` | `pj::StreamStat::rtcp.txStat.pkt` | Transmitter-side packet count supplied by PJMEDIA |
| `[RTP] loss` | `RtpStatistics` | `100 * lost / (received + lost)`; zero when the denominator is zero |
| `[RTP] jitter` | `rtcp.rxStat.jitterUsec.mean / 1000` | Mean receive jitter, in milliseconds |
| `[RTP] rtt` | `rtcp.rttUsec.mean / 1000` | Mean RTCP round-trip time, in milliseconds |
| `[RTP] tx-seq`, `tx-ts` | `rtcp.rtpTxLastSeq`, `rtcp.rtpTxLastTs` | Last transmit sequence and RTP timestamp reported by PJMEDIA |

Negative timing values are clamped to zero by the adapter. A zero timing/counter value can also mean that a meaningful estimate is not yet available; it must not automatically be described as proof of perfect network quality. The diagnostics snapshot is not an independent packet capture, and a successful `audio=yes` flag is not a substitute for listening in both directions.

## Runtime and lifecycle

For both outgoing and incoming calls, the client initializes the endpoint, creates the SIP transport, starts PJSUA2, configures capture/playback devices, and then creates/registers the account. Outgoing creates a tracked call and dials; incoming constructs its tracked call from the ID supplied by `onIncomingCall()` and answers after the configured delay.

Call-state and media-state callbacks have distinct jobs. `onCallState()` records confirmation/disconnection; `onCallMediaState()` checks for active audio, makes both conference-bridge connections, and records `media_connected=true` only after wiring succeeds. Shared results are synchronized with mutexes and condition variables. Callback error handlers record failures rather than allowing exceptions to escape PJSUA2 callback boundaries.

The CLI waits until the call is confirmed **and** media is connected (or a failure/timeout occurs), optionally holds the call, and snapshots `[MEDIA]` and `[RTP]` **before** requesting hangup. An empty active-audio diagnostics result is reported as an error by the CLI. Reading the stream after the call has been destroyed would be invalid.

`rtp_diagnostics()` synchronizes with call callbacks using the existing call-callback mutex. Teardown preserves the Phase 5 sequencing: request hangup, wait for disconnection, serialize with callbacks before destroying the tracked call object, allow the PJSUA call slot to become free, shut down the account without forced deletion, then destroy the endpoint. The code relies on PJSUA2/PJMEDIA's media lifecycle; it does not implement a separate manual RTP transport or custom conference-bridge teardown routine.

## Verification strategy

Run `./scripts/phase6/verify.sh` with the locally installed PJPROJECT 2.17 visible through `PKG_CONFIG_PATH`. The script performs audio preflight, reuses the complete Phase 5 gate (format, dependency check, PJSIP and non-PJSIP builds/tests, sanitizers, and clang-tidy), then requires at least four `RtpStatisticsTest` cases and three `Pjsua2RtpDiagnosticsTest` cases. Discovery counts prevent a missing focused test suite from giving a vacuous pass.

Automated tests validate the device mapping/selection and active-media index policy, pure packet-loss arithmetic, PJSUA2 stream-stat mapping, and the existing SIP CLI contracts. They do not create a substitute for the real Asterisk/handset test.

The separate live gate makes one call in each direction between the local SIP client (`1002`) and the phone (`1001`). For **each** call, confirm registration (`200 OK`), `CONFIRMED`, `connected=yes audio=yes`, audible speech both ways, nonzero received and sent packet counts, consistent `[MEDIA]`/`[RTP]` output, and clean disconnection. Packet loss and jitter are observations, not fixed acceptance thresholds for all networks.

The retained Phase 6 evidence records the acceptance observations, including operator-confirmed bidirectional audio and read-only RTP diagnostics. The original development runs reported outgoing PCMU/8 kHz with RX 1,062 / TX 937 / loss 0% and incoming PCMU/8 kHz with RX 946 / TX 946 / loss 0%. Both held for approximately 20 seconds. These are historical local observations, not required thresholds or reproducible performance baselines; use `evidence/phase6/README.md` for the accepted evidence index.

## Design rationale

- Preserve the proven Phase 5 signaling, callback synchronization, and teardown model instead of rebuilding it for audio.
- Select devices by stable name and optional driver; numeric device enumeration can change after reboot or hot-plugging.
- Let PJSUA2/PJMEDIA own conference mixing, codecs, and RTP transport; only bridge the application's chosen audio devices to the active call stream.
- Keep the arithmetic `RtpStatistics` independent of PJSUA2 to permit non-PJSIP unit tests; isolate library-specific conversion in the PJSUA2 adapter.
- Snapshot statistics while the call is still active, and keep actual human listening as an explicit acceptance gate.

## Phase boundary

Phase 5's null device was sufficient to prove SIP registration and call lifecycle. Phase 6 adds live capture/playback, bidirectional conference-bridge connections, media-connected state, read-only stream statistics, and automated/live validation. This milestone did not include production-grade long-running call orchestration or an encryption policy. A separate Qt/QML interface was subsequently accepted in Phase 7; its design and evidence are documented under `docs/architecture/qt-qml.md` and `evidence/phase7/README.md`.

## References

- `docs/phase6/README.md`
- `docs/phase5/README.md`
- `evidence/phase6/README.md`
- `docs/architecture/pjsip-cli.md`
- `docs/interview/phase6-questions.md`
- `scripts/phase6/verify.sh`
- `scripts/phase6/verify-audio-environment.sh`
- `src/pjsip/Pjsua2AudioDeviceManager.*`
- `src/pjsip/Pjsua2CallAudioBridge.*`
- `src/pjsip/Pjsua2RtpDiagnostics.*`
- `src/network/RtpStatistics.*`
- `src/pjsip/SipOutgoingCallClient.*`
- `src/pjsip/SipIncomingCallClient.*`
