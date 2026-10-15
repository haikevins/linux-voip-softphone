# Phase 6 Architecture — Audio and RTP Media

## Status

Completed — milestone `v0.7-rtp-audio`.

## Purpose

Extend Phase 5's PJSUA2 call clients with Linux sound devices, two-way media routing and observable RTP statistics without reimplementing RTP or weakening the established SIP lifecycle.

The principal components are:

```text
src/pjsip/
├── SipCallAudioConfig.hpp
├── Pjsua2AudioDeviceManager.*
├── Pjsua2CallAudioBridge.*
├── Pjsua2RtpDiagnostics.*
├── SipOutgoingCallClient.*
└── SipIncomingCallClient.*

src/network/
└── RtpStatistics.*

src/app/
├── sip_call_main.cpp
└── sip_listen_main.cpp
```

## Scope

- Select capture/playback devices through the PJSUA2 device manager.
- Connect active call media in both directions using the PJSUA2 conference bridge.
- Coordinate call-media callbacks with synchronous CLI execution.
- Read RTP/RTCP stream information without interfering with RTP transport.
- Keep device checks, automated tests and live listening as separate verification layers.

Not covered: custom RTP sockets, audio mixing policy, echo cancellation tuning, SRTP policy, multi-call orchestration, or the Qt/QML interface.

## Architecture model

### Layering and responsibility

```text
sip_call_main / sip_listen_main
    |
    v
SipOutgoingCallClient / SipIncomingCallClient
    |  own endpoint, account, call and callback state
    |
    +--> Pjsua2AudioDeviceManager --> runtime capture / playback IDs
    |
    +--> onCallMediaState() --> Pjsua2CallAudioBridge
    |                            capture --> call --> RTP transmit
    |                            RTP receive --> call --> playback
    |
    +--> Pjsua2RtpDiagnostics --> pj::Call stream info/statistics
                                   |
                                   +--> RtpStatistics (arithmetic only)
```

SIP/SDP handles signaling and codec negotiation; PJSUA2/PJMEDIA owns the conference bridge, codec and RTP/RTCP streams. The Phase 3 `UdpSocket` is independent and is **not** used to implement another call-media transport.

### Audio-device selection

`SipCallAudioConfig` supplies separate `AudioDeviceSelector` settings for microphone capture and playback. Each defaults to the name `default`, with an optional driver qualifier such as `ALSA`.

The adapter enumerates `pj::AudDevManager::enumDev2()`, resolves a device with the required capture/playback capability, and applies the resulting runtime ID through `setCaptureDev()` and `setPlaybackDev()`. Keeping device IDs out of stored configuration avoids depending on enumeration order across reboots or hot-plug events.

The preflight script checks the PJPROJECT dependency and requires available capture/playback capabilities. This is a **capability** check: a real call and listening test are still needed to confirm the chosen headset and the actual audio route.

### Bidirectional media connection

`active_audio_media_indices()` filters the call's media entries to active audio streams (`PJSUA_CALL_MEDIA_ACTIVE`). The media bridge uses `AudioMedia::startTransmit()` for both links:

```text
microphone --> capture AudioMedia --> call AudioMedia --> RTP send
speaker   <-- playback AudioMedia <-- call AudioMedia <-- RTP receive
```

The transmit API defines source-to-destination flow. Wiring only one link can create one-way audio even when SIP signaling has succeeded.

### RTP/media diagnostics

`Pjsua2RtpDiagnostics` obtains information for active audio media through `pj::Call::getStreamInfo()` and `getStreamStat()`. `RtpStatistics` provides independent arithmetic for calculated fields; neither class reads or sends raw RTP packets.

| CLI output | Source | Meaning |
|---|---|---|
| `[MEDIA] codec`, `clock-rate`, `rx-pt`, `tx-pt` | `pj::StreamInfo` | Negotiated codec and payload types at snapshot time |
| `[RTP] received`, `lost` | `pj::StreamStat::rtcp.rxStat` | Receiver-side statistics reported by PJMEDIA |
| `[RTP] sent` | `pj::StreamStat::rtcp.txStat.pkt` | Transmitted packets reported by PJMEDIA |
| `[RTP] loss` | `RtpStatistics` | `100 * lost / (received + lost)`; zero for a zero denominator |
| `[RTP] jitter` | `rtcp.rxStat.jitterUsec.mean / 1000` | Mean receive jitter in milliseconds |
| `[RTP] rtt` | `rtcp.rttUsec.mean / 1000` | Mean RTCP round-trip time in milliseconds |
| `[RTP] tx-seq`, `tx-ts` | `rtcp.rtpTxLastSeq`, `rtcp.rtpTxLastTs` | Last transmitted RTP sequence/timestamp reported by PJMEDIA |

The adapter clamps negative timing values to zero. A displayed zero can also mean that sufficient measurements are unavailable; it is not proof of perfect network quality. Media flags and counters cannot replace audible listening tests or independent packet inspection.

## Runtime and lifecycle

### Starting the audio-enabled client

```text
Endpoint::libCreate / libInit
           |
           v
create SIP transport; Endpoint::libStart
           |
           v
resolve and set capture / playback devices
           |
           v
create + register account
           |
           +--> outgoing: makeCall()
           |
           +--> incoming: onIncomingCall() / answer
           v
onCallMediaState() --> connect both audio directions
           |
           v
confirmed call + media_connected
           |
           v
snapshot [MEDIA] and [RTP] while call is active
           |
           v
hangup --> disconnected --> ordered cleanup
```

`onCallState()` tracks call confirmation and disconnection. `onCallMediaState()` connects active audio and sets `media_connected` only after successful bridge configuration. Callback state is synchronized through the existing mutex/condition-variable model; callback errors are recorded instead of escaping across the PJSUA2 callback boundary.

The CLI uses bounded waits for signaling and media readiness. A missing active audio stream at diagnostics time is an error rather than a successful empty measurement.

### Shutdown and ownership

Diagnostic reads synchronize with call callbacks and occur before hangup. Teardown follows Phase 5's ownership rules: request hangup, wait for disconnection, synchronize with callbacks, release the tracked call safely, allow the underlying call slot to become free, shut down the account without forced deletion, then destroy the endpoint. PJSUA2/PJMEDIA remains responsible for its own RTP and conference-bridge resources.

## Verification strategy

Automated verification uses:

```bash
./scripts/phase6/verify.sh
```

This performs the audio preflight, reuses the Phase 5 verification gates, and exercises focused RTP arithmetic, PJSUA2 diagnostics, device-selection behavior, media-index policy and CLI contracts. The phase verifier also checks that the focused test suites are actually discovered rather than silently running no cases.

Live tests require one outgoing and one incoming Asterisk call. For each direction verify registration, confirmed signaling, active negotiated audio, nonzero RX/TX packet counters, actual intelligible speech in both directions, and clean disconnection. `docs/phase6/README.md` contains the reproducible CLI commands; `evidence/phase6/` contains the observed acceptance results.

The separate Wireshark study inspects selected RTP streams during one controlled outgoing call. It is useful for reasoning about sequence numbers, timestamps and observed pauses, but cannot establish universal packet-loss or silence-suppression behavior.

## Design rationale

- Reuse the Phase 5 signaling and callback lifecycle rather than introducing a parallel call controller.
- Resolve audio devices by name/driver and capability instead of a hard-coded numeric index.
- Use PJMEDIA's existing bridge and RTP implementation rather than a second media transport.
- Separate pure RTP statistics calculations from PJSUA2-specific field mapping for testability.
- Capture media diagnostics before teardown, when the call and streams still exist.
- Treat preflight, runtime counters, packet inspection and human listening as complementary checks.

## Phase boundary

Phase 5 validated SIP registration and call state using a null audio device. Phase 6 adds real capture/playback selection, call-media routing, observable RTP/RTCP statistics and confirmed two-way audio in the local test environment. Long-running call control, security policy, audio enhancement and Qt/QML integration remain outside this architecture's scope.

## References

- `docs/phase6/README.md`
- `docs/phase5/README.md`
- `docs/architecture/pjsip-cli.md`
- `docs/protocols/rtp.md`
- `docs/interview/phase6-questions.md`
- `scripts/phase6/verify.sh`
- `evidence/phase6/README.md`
