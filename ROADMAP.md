# Roadmap

This document records the major implementation milestones of Linux VoIP Softphone.
Detailed design notes, test procedures, and verification artifacts live under
`docs/` and `evidence/`.

## Status

The planned engineering roadmap is complete through `v1.0.0`.
There is no Phase 12 planned at this time.

## Milestones

| Phase | Focus | Release |
| --- | --- | --- |
| 0 | Repository and engineering environment | `v0.1-project-skeleton` |
| 1 | Modern C++ foundation | `v0.2-cpp-core` |
| 2 | Linux system programming and IPC | `v0.3-linux-system` |
| 3 | TCP/UDP networking | `v0.4-networking` |
| 4 | SIP and Asterisk lab | `v0.5-sip-asterisk` |
| 5 | PJSIP CLI softphone | `v0.6-pjsip-cli` |
| 6 | RTP and Linux audio | `v0.7-rtp-audio` |
| 7 | Qt/QML desktop GUI | `v0.8-qt-gui` |
| 8 | Multithreading and lifetime hardening | — |
| 9 | Debugging and diagnostics | `v0.9-diagnostics` |
| 10 | RTSP/SDP/RTP mini-lab | `v0.10-rtsp-lab` |
| 11 | Final engineering release | `v1.0.0` |

## Engineering progression

Phases 0–3 established the C++20, Linux, build, testing, IPC, and networking
foundation. Phases 4–7 added SIP signaling, PJSIP integration, RTP audio, and
the Qt/QML desktop client.

Phases 8–9 focused on thread ownership, shutdown behavior, lifetime safety,
diagnostics, sanitizers, and crash analysis. Phase 10 added an isolated RTSP
protocol lab without coupling it to the production softphone. Phase 11 closed
the project with release builds, packaging, system acceptance, and final
documentation.

## Current release

`v1.0.0` is the completed release baseline. It includes the desktop softphone,
CLI SIP tools, RTP audio, diagnostics, RTSP lab, automated verification, and
release packaging.

Release history is summarized in [`CHANGELOG.md`](CHANGELOG.md).

## Supporting documentation

- Architecture and protocol notes: [`docs/`](docs/)
- Verification records: [`evidence/`](evidence/)
- Build and usage overview: [`README.md`](README.md)

## Post-release work

Small README, documentation, and demo updates can continue as normal commits
without moving the `v1.0.0` tag. Code fixes or new features should be released
under a new version when appropriate.
