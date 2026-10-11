# Phase 6 — RTP and Audio

## Status

**Completed** — milestone tag `v0.7-rtp-audio`

Accepted for the documented local Asterisk lab.

Accepted evidence includes automated verification, outgoing and incoming calls with operator-confirmed two-way audio, RTP/RTCP diagnostics, and one controlled Wireshark RTP study. These observations apply to the recorded workstation, devices, network, and revisions; they are not a guarantee for other environments.

## Objective

Extend the Phase 5 SIP call lifecycle to real Linux capture/playback, verified two-way audio, and read-only RTP/media diagnostics without regressing signaling or teardown behavior.

## Scope

Phase 6 covers:

- audio-device capability preflight and runtime selection;
- bidirectional PJSUA2 conference-media routing;
- RTP/RTCP statistics presentation;
- focused automated verification;
- outgoing and incoming live-call validation;
- a controlled wire-level RTP inspection.

PJSUA2/PJMEDIA owns the call RTP transport and codecs. The project's general-purpose UDP abstraction is not used as a replacement RTP stack.

## Deliverables

- `scripts/phase6/verify-audio-environment.sh` — PJPROJECT/audio-device preflight.
- `src/pjsip/Pjsua2AudioDeviceManager.*` and `SipCallAudioConfig.hpp` — capture/playback selection.
- `src/pjsip/Pjsua2CallAudioBridge.*` — call-to-device and device-to-call routing.
- `src/network/RtpStatistics.*` and `src/pjsip/Pjsua2RtpDiagnostics.*` — RTP/media diagnostics.
- Updated SIP call/listen CLIs with media and diagnostics reporting.
- Focused GoogleTests and the aggregate Phase 6 verification wrapper.
- Sanitized evidence under `evidence/phase6/`.

## Architecture and design

SIP establishes the dialog and SDP negotiates media parameters. PJSUA2/PJMEDIA owns the active audio stream and RTP transport. The application resolves capture/playback devices at runtime and, when call media becomes active, connects both directions through `AudioMedia::startTransmit()`.

The CLI waits for confirmed signaling and active media, captures diagnostic snapshots while the call is still alive, then performs the established Phase 5 teardown sequence.

See `docs/architecture/audio-rtp.md` for lifecycle and component details.

## Build and run

Expose the local PJPROJECT 2.17 installation and run the complete automated gate:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase6/verify.sh
```

Build the PJSIP applications directly with:

```bash
cmake --preset pjsip-debug
cmake --build --preset pjsip-debug -j
build/pjsip-debug/src/voip-sip-call --help
build/pjsip-debug/src/voip-sip-listen --help
```

For live calls, use the existing Asterisk lab and runtime audio selectors. Supply the SIP password only through the configured environment-variable flow; never place credentials in argv, evidence, screenshots, or committed logs.

## Verification

Automated verification uses:

```bash
./scripts/phase6/verify.sh
```

The recorded Phase 6 gate passed the inherited Phase 5 checks plus audio preflight, RTP-statistics tests, and PJSUA2 diagnostics tests.

Manual verification covered both call directions and required:

- successful registration and call confirmation;
- actual intelligible speech in both directions;
- active `[MEDIA]` output;
- nonzero transmitted and received RTP packets;
- clean disconnection.

A controlled approximately 75-second outgoing call was also inspected with Wireshark. The selected opposite-direction RTP streams showed PCMU, regular 160-tick/20-ms timestamp progression, continuous sequence behavior, and zero observed loss/sequence errors in the selected analysis. The raw capture remains outside Git.

A SIP `200 OK`, `audio=yes`, or nonzero RTP counters alone do not prove audible two-way speech.

## Evidence

The evidence index is `evidence/phase6/README.md`. It records automated verification, live-call observations, codec/RTP summaries, operator-confirmed two-way audio, and the sanitized Wireshark study.

Do not commit SIP passwords, Digest authorization material, raw secret-bearing SIP/PJSIP logs, SRTP keying material, raw audio, or `.pcap`/`.pcapng` captures containing sensitive traffic.

## Interview focus

Be able to explain:

- why SIP call success does not prove audible media;
- why audio devices are resolved at runtime instead of by fixed numeric ID;
- how the two conference-media transmission directions are connected;
- why RTP statistics are read before teardown;
- what loss, jitter, and RTT snapshots do and do not prove;
- why automated tests, runtime counters, packet inspection, and human listening are separate verification layers.

See `docs/interview/phase6-questions.md`.

## Exit criteria

Phase 6 is accepted when the documented environment has:

- successful outgoing and incoming calls with two-way intelligible audio;
- active codec/media and RTP observations;
- clean teardown;
- passing Phase 6 automated verification;
- architecture, protocol, interview, and evidence documentation;
- sanitized wire-level evidence with clear limitations.

The annotated `v0.7-rtp-audio` tag is the Phase 6 release milestone.

## References

- `docs/architecture/audio-rtp.md`
- `docs/protocols/rtp.md`
- `docs/architecture/pjsip-cli.md`
- `docs/interview/phase6-questions.md`
- `docs/phase5/README.md`
- `scripts/phase6/verify.sh`
- `scripts/phase6/verify-audio-environment.sh`
- `evidence/phase6/README.md`

## Next phase

Phase 7 adds the Qt/QML user interface on top of the accepted SIP and audio runtime without changing the recorded Phase 6 acceptance boundary.
