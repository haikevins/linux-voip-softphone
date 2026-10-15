# Phase 6 — RTP and Audio

## Status

**Completed** — milestone `v0.7-rtp-audio`.

Phase 6 extends the Phase 5 PJSIP CLI softphone with real Linux audio capture/playback, bidirectional call media, and RTP diagnostics. Both outgoing and incoming calls passed the documented local Asterisk lab's automated and human-listening checks.

## Objective

Enable intelligible, two-way audio over an established SIP call while preserving the existing callback synchronization, bounded waits, and safe call teardown.

## Scope

Included in this phase:

- Linux audio capture/playback capability preflight;
- device selection by name and optional driver instead of fixed numeric IDs;
- bidirectional PJSUA2 call-media routing;
- active audio and negotiated codec reporting;
- read-only RTP/RTCP statistics;
- unit/integration regression and quality-gate verification;
- outgoing/incoming live calls with operator-confirmed listening;
- sanitized RTP packet-analysis evidence from one controlled call.

Deliberately excluded:

- a custom RTP stack or raw UDP media transport;
- production audio processing, echo-cancellation policy, or multi-call mixing;
- guaranteed network quality or universal device compatibility;
- SRTP policy and Qt/QML user interface.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| Audio preflight | Check library and capture/playback availability | `scripts/phase6/verify-audio-environment.sh` |
| Device selection | Resolve runtime capture/playback devices | `src/pjsip/Pjsua2AudioDeviceManager.*`, `src/pjsip/SipCallAudioConfig.hpp` |
| Media bridge | Connect microphone and speaker to active call media | `src/pjsip/Pjsua2CallAudioBridge.*` |
| RTP diagnostics | Read stream statistics and calculate loss ratios | `src/pjsip/Pjsua2RtpDiagnostics.*`, `src/network/RtpStatistics.*` |
| CLI integration | Report media results for outgoing/incoming calls | `src/app/sip_call_main.cpp`, `src/app/sip_listen_main.cpp` |
| Verification | Run preflight, regressions, sanitizers and focused tests | `scripts/phase6/verify.sh` |
| Architecture note | Describe media ownership, routing and lifecycle | `docs/architecture/audio-rtp.md` |
| Interview review | Review RTP, media callbacks and diagnostics | `docs/interview/phase6-questions.md` |
| Evidence | Record sanitized verification and live-call results | `evidence/phase6/` |

## Architecture and design

The signaling and audio paths have distinct responsibilities:

```text
CLI (outgoing / incoming)
           |
           v
SipOutgoingCallClient / SipIncomingCallClient
           |
           +--> PJSUA2 SIP + SDP negotiation --> Asterisk / remote endpoint
           |
           +--> audio device manager --> capture / playback devices
           |
           +--> onCallMediaState() --> PJSUA2 conference bridge
           |                           capture --> call --> RTP transmit
           |                           RTP receive --> call --> playback
           |
           +--> active stream --> RTP/RTCP diagnostics --> [MEDIA] / [RTP]
```

Important design rules:

- PJSUA2/PJMEDIA owns the codec and RTP transport; the Phase 3 `UdpSocket` is not reused as a second RTP implementation.
- Capture-to-call and call-to-playback are connected separately with `AudioMedia::startTransmit()`; both directions are required.
- The CLI waits for confirmed signaling **and** successful media connection.
- Stream information is read while the call is active, before hangup.
- Existing Phase 5 callback synchronization, timeout handling and teardown order are preserved.
- An available device or nonzero RTP counter does not by itself prove audible, intelligible speech.

See `docs/architecture/audio-rtp.md` for lifecycle and diagnostic-field details.

## Build and run

The documented workstation uses PJPROJECT/PJSIP 2.17. Make its pkg-config metadata available before configuring:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
```

Build the PJSIP applications and inspect their CLI options:

```bash
cmake --preset pjsip-debug
cmake --build --preset pjsip-debug -j
build/pjsip-debug/src/voip-sip-call --help
build/pjsip-debug/src/voip-sip-listen --help
```

The existing lab uses Asterisk on `127.0.0.1`, workstation account `1002`, and phone account `1001`. Supply the SIP password via the configured environment variable (default: `VOIP_SIP_PASSWORD`), never through CLI arguments or committed files.

Outgoing call from `1002` to `1001`:

```bash
build/pjsip-debug/src/voip-sip-call \
    --username 1002 --server 127.0.0.1 --target 1001 --transport udp \
    --capture-device default --capture-driver ALSA \
    --playback-device default --playback-driver ALSA \
    --call-timeout-ms 30000 --hold-ms 20000
```

Incoming call to `1002` (start the listener, then dial from `1001`):

```bash
build/pjsip-debug/src/voip-sip-listen \
    --username 1002 --server 127.0.0.1 --transport udp \
    --capture-device default --capture-driver ALSA \
    --playback-device default --playback-driver ALSA \
    --incoming-timeout-ms 60000 --call-timeout-ms 15000 \
    --answer-delay-ms 1000 --hold-ms 20000
```

`default` is resolved at runtime. Confirm that the selected capture and playback devices match the intended headset or speaker before interpreting listening results.

## Verification

Run the complete automated gate:

```bash
./scripts/phase6/verify.sh
```

It includes:

- audio-device and PJPROJECT preflight;
- inherited Phase 5 PJSIP/non-PJSIP regressions;
- formatting, `clang-tidy`, ASan and UBSan checks;
- focused RTP statistics and PJSUA2 diagnostics tests.

The recorded Phase 6 run passed these gates. Exact observed counts and the tested revisions belong to `evidence/phase6/automated-verification.txt` rather than being treated as permanent suite sizes.

Live validation additionally requires, **for both call directions**:

- successful registration and confirmed call signaling;
- active negotiated audio (`[MEDIA]`, including codec/clock rate);
- nonzero RTP sent and received packet counters (`[RTP]`);
- a person hearing intelligible audio in both directions;
- clean hangup and shutdown.

A separate approximately 75-second outgoing-call Wireshark study inspected two opposite-direction RTP streams, including an operator-reported silence interval. It is supporting packet-level evidence, **not** a replacement for the incoming-call check or a guarantee of zero loss on other networks.

## Evidence

Committed summaries are indexed under:

```text
evidence/phase6/
```

They cover automated verification, live registration, outgoing and incoming calls, a phase summary, and the separate Wireshark RTP study. Runtime secrets, raw SIP authentication traces, and `.pcap`/`.pcapng` captures must not be committed; PCMU payloads may contain intelligible speech.

## Interview focus

Use `docs/interview/phase6-questions.md` together with the code. Be ready to explain:

- SIP/SDP negotiation versus the actual audio path;
- why audio device IDs are resolved at runtime;
- the two `startTransmit()` directions;
- the purpose of `onCallMediaState()` and `media_connected`;
- why RTP statistics must be read before teardown;
- the meaning and limitations of loss, jitter and RTT;
- why a SIP success code or RTP packet count cannot prove audible speech;
- the difference between automated tests, packet analysis and human listening.

## Exit criteria

Phase 6 is complete for the documented lab when:

- capture and playback selection works on the test workstation;
- both outgoing and incoming calls provide audible two-way speech;
- active codec and RTP statistics are reported before hangup;
- call shutdown preserves Phase 5 safe teardown;
- the automated verifier and quality gates pass;
- architecture, protocol, interview and sanitized evidence are present;
- evidence records the scope and limitations of the controlled RTP study.

The milestone identifier is `v0.7-rtp-audio`; the presence or publication of its Git tag is a separate repository-state check, not an audio acceptance measurement.

## References

- Architecture: `docs/architecture/audio-rtp.md`
- RTP protocol: `docs/protocols/rtp.md`
- Prior architecture: `docs/architecture/pjsip-cli.md`
- Interview review: `docs/interview/phase6-questions.md`
- Evidence: `evidence/phase6/`

## Next phase

Phase 7 introduces the Qt/QML interface on top of the verified SIP and audio runtime.
