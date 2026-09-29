# Phase 6 — RTP and audio

## Status

In progress. Commit 1 establishes the Linux/PJSUA2 audio-device preflight before call media is connected to real capture and playback devices.

## Objective

Enable reliable two-way audio for the existing SIP call lifecycle by connecting negotiated PJSIP call media to Linux audio capture and playback while preserving the Phase 5 signaling behavior.

## Scope

Phase 6 covers the runtime media path after SIP/SDP negotiation: PJSUA2 audio devices, call audio media, conference-bridge connections, RTP-backed audio flow, device selection, media lifecycle handling, and verification of real two-way audio.

Commit 1 is deliberately narrower. It verifies that the configured PJPROJECT installation can compile/link PJSUA2 audio code and that the runtime exposes at least one capture-capable and one playback-capable audio device. It does not modify the existing call applications or replace their Phase 5 null-audio behavior yet.

## Deliverables

Current deliverable:

- `scripts/phase6/verify-audio-environment.sh` — compile/link and runtime preflight for PJSUA2 audio-device enumeration.

Planned Phase 6 deliverables include real call-media attachment, safe media teardown, focused automated verification where practical, and sanitized live-call evidence.

## Architecture and design

SIP signaling and RTP/audio remain separate responsibilities. SIP establishes and tears down the dialog and negotiates media parameters through SDP. PJSIP/PJMEDIA owns the RTP media stream and exposes the active call audio as PJSUA2 `AudioMedia`. The application will connect that call media to the PJSUA2 capture and playback device media rather than using the Phase 5 null device.

Commit 1 establishes the hardware/runtime boundary first. Device enumeration must succeed before later commits make real audio devices part of the call lifecycle.

## Build and run

Use the same PJPROJECT 2.17 installation used by Phase 5:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase6/verify-audio-environment.sh
```

The preflight compiles a temporary C++20 PJSUA2 program using `pkg-config --static --libs libpjproject`, starts the PJSUA2 endpoint, enumerates audio devices, and requires both capture and playback capability.

## Verification

Commit 1 is complete when:

- `libpjproject >= 2.17` is visible through `pkg-config`;
- the PJSUA2 audio-device smoke program compiles and links;
- runtime audio-device enumeration succeeds;
- at least one capture-capable device is present;
- at least one playback-capable device is present;
- the existing Phase 5 automated verification remains green.

No live SIP call is required for this commit.

## Evidence

Commit 1 does not add committed Phase 6 evidence. Its output is an environment preflight, not yet proof of an end-to-end RTP/audio call. Phase 6 evidence should be added only after the media path is implemented and verified, and must follow the repository evidence retention and secret-redaction policy.

## Interview focus

Be able to explain the difference between SIP signaling, SDP negotiation, RTP media transport, the PJSUA2 conference bridge, call `AudioMedia`, capture/playback device media, and the Phase 5 null audio device.

Also be able to explain why device enumeration is validated before wiring hardware audio into call lifecycle callbacks.

## Exit criteria

Phase 6 exits only when outgoing and incoming calls can carry verified two-way audio through the Linux audio devices, media connections are established and removed safely with the call lifecycle, regressions remain green, and sanitized evidence records the final result.

## References

- `docs/architecture/pjsip-cli.md`
- `docs/protocols/sip.md`
- `docs/phase5/README.md`
- `scripts/phase5/verify.sh`
- PJPROJECT/PJSUA2 audio device manager documentation

## Next phase

Phase 7 introduces the Qt/QML user interface on top of the verified signaling and audio runtime.
