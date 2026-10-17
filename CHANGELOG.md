# Changelog

This file summarizes the major engineering milestones of Linux VoIP Softphone.
Detailed architecture, verification, and evidence remain under `docs/` and
`evidence/`.

## [v1.0.0](https://github.com/haikevins/linux-voip-softphone/releases/tag/v1.0.0)

- Final engineering release with reproducible Release builds, installation layout, and TGZ packaging.
- Added packaged-artifact acceptance covering executable presence, runtime dependencies, CLI/RTSP smoke checks, and Qt GUI startup.
- Finalized system architecture, operational documentation, portfolio README, demo guide, and release evidence.

## [v0.10-rtsp-lab](https://github.com/haikevins/linux-voip-softphone/releases/tag/v0.10-rtsp-lab)

- Added RTSP message/parsing primitives and a reusable RTSP client-session workflow.
- Added SDP-driven RTP/AVP UDP media reception and end-to-end RTSP verification.
- Documented the RTSP protocol flow and milestone evidence.

## [v0.9-diagnostics](https://github.com/haikevins/linux-voip-softphone/releases/tag/v0.9-diagnostics)

- Added debugging and diagnostics workflows for crash analysis and regression verification.
- Strengthened lifetime/error-path checks and retained focused diagnostic tooling.
- Documented the debugging architecture and verification evidence.

## [v0.8-qt-gui](https://github.com/haikevins/linux-voip-softphone/releases/tag/v0.8-qt-gui)

- Added the Qt 6/QML desktop interface on top of the existing SIP/media runtime.
- Added live SIP registration, outgoing/incoming calls, Answer/Decline, call duration, and reusable registered state.
- Added live RTP/audio diagnostics while keeping QML separated from PJSUA2 ownership and worker-thread execution.

## [v0.7-rtp-audio](https://github.com/haikevins/linux-voip-softphone/releases/tag/v0.7-rtp-audio)

- Added real Linux capture/playback and bidirectional RTP audio to the PJSIP call path.
- Added RTP/RTCP diagnostics including codec, packet, loss, jitter, and RTT observations.
- Verified outgoing and incoming calls with two-way audio in the controlled Asterisk lab.

## [v0.6-pjsip-cli](https://github.com/haikevins/linux-voip-softphone/releases/tag/v0.6-pjsip-cli)

- Added PJPROJECT/PJSIP 2.17 integration and CLI-oriented SIP workflows.
- Added SIP registration plus outgoing and incoming call lifecycle executables.
- Established the PJSIP regression baseline later reused by the RTP/audio and Qt milestones.
