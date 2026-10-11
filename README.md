# Linux VoIP Softphone

A phase-driven Linux SIP softphone engineering project built in C++20 with CMake, PJSIP/PJSUA2, Qt/QML, Linux networking/system programming, automated tests, sanitizers, static analysis, and retained verification evidence.

## Current status

Phase 0 through Phase 7 are accepted for their documented environments. The latest published milestone is the annotated tag `v0.8-qt-gui` for Phase 7.

Phase 8 — Multithreading is merged to `main` and post-merge automated verification passes. Final maintainer acceptance is still **PENDING**. The following PJSUA2 boundaries remain unchanged:

- live external callback quiescence: **UNPROVEN**;
- production-client real `PJ_EBUSY` recovery: **UNPROVEN**;
- PJSUA2 integration readiness: **BLOCKED**.

Phase 8 has no separate release tag in the current roadmap. `v0.9-diagnostics` belongs to Phase 9.

## Implemented

- C++20 project/toolchain foundation with CMake, GoogleTest, clang-format, clang-tidy, ASan, and UBSan.
- RAII/value-semantics labs plus a tested call-state core.
- Linux process, signal, file-descriptor RAII, and Unix-domain-socket IPC work.
- IPv4 endpoint, TCP client/server, and UDP socket abstractions with integration tests.
- Local Asterisk SIP lab and documented REGISTER/call flows.
- PJPROJECT 2.17 / PJSUA2 integration behind `ENABLE_PJSIP`.
- CLI registration plus outgoing/incoming call lifecycles.
- Real Linux two-way audio with RTP/RTCP diagnostics.
- Qt/QML GUI with registration, outgoing/incoming calls, Answer/Decline, hangup, duration, and live RTP/audio diagnostics.
- Phase 8 `ThreadSafeQueue<T>` and owned `SerialTaskWorker` concurrency primitives.
- Repeated 100,000-event MPMC stress verification and standalone Clang 18 ThreadSanitizer coverage.
- Hardened Qt/PJSUA2 ownership, callback-lifetime, and shutdown-order contracts with fail-closed handling where safe teardown cannot be proven.

Future work includes multiple accounts, call waiting, mute/hold, audio-device selection UI, persistent call history, diagnostics/debugging, RTSP, and final release hardening.

## Repository layout

```text
cmake/          reusable CMake modules
config/         runtime examples and Asterisk lab configuration
docs/           architecture, protocols, phase guides, and interview review
evidence/       scrubbed verification evidence by milestone
labs/           focused C++/Linux learning programs
scripts/        build, quality, and phase verification workflows
src/            production C++ code
tests/          unit, integration, and Qt tests
```

See [`ROADMAP.md`](ROADMAP.md) for phase planning and milestone status.

## Build and test

Base build, without PJPROJECT:

```bash
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug --output-on-failure
```

For local PJPROJECT 2.17:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake --preset pjsip-debug
cmake --build --preset pjsip-debug -j
./scripts/phase5/verify-pjsip-dependency.sh
./scripts/phase6/verify.sh
```

Qt-only GUI:

```bash
cmake --preset qt-gui-debug
cmake --build --preset qt-gui-debug --clean-first -j2
```

Qt + PJSIP GUI:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake --preset qt-pjsip-gui-debug
cmake --build --preset qt-pjsip-gui-debug --clean-first -j2
```

The combined GUI accepts `--sip-register USER SERVER [PORT]`. Supply SIP passwords only through the existing hidden stdin flow; never place credentials in argv, source files, evidence, screenshots, or committed logs.

## Verification

The repository treats a phase as complete only when the required source, automated verification, runtime evidence, and written explanation exist for that phase.

Useful entry points:

```bash
./scripts/dev/build.sh
./scripts/dev/test.sh
./scripts/quality/verify-docs-structure.sh
./scripts/quality/verify-evidence-structure.sh
./scripts/phase8/verify.sh
```

Historical `docs/`, `labs/`, `scripts/`, and `evidence/` artifacts are intentionally retained as part of the engineering record. Generated build trees are not committed.

## Milestones

| Tag | Phase |
|---|---|
| `v0.1-project-skeleton` | Phase 0 — repository and engineering environment |
| `v0.2-cpp-core` | Phase 1 — modern C++ foundation |
| `v0.3-linux-system` | Phase 2 — Linux system programming and IPC |
| `v0.4-networking` | Phase 3 — TCP/UDP networking |
| `v0.5-sip-asterisk` | Phase 4 — SIP + Asterisk lab |
| `v0.6-pjsip-cli` | Phase 5 — PJSIP CLI softphone |
| `v0.7-rtp-audio` | Phase 6 — RTP + Audio |
| `v0.8-qt-gui` | Phase 7 — Qt/QML GUI |

Phase 9 begins only after explicit Phase 8 maintainer acceptance.

## Documentation

- [`docs/README.md`](docs/README.md) — documentation taxonomy and schemas.
- [`ROADMAP.md`](ROADMAP.md) — phase roadmap and current progress.
- `docs/architecture/` — system/component design and lifecycle.
- `docs/protocols/` — protocol behavior and observations.
- `docs/interview/` — phase review questions.
- `docs/phase*/README.md` — primary phase entry points.
- [`evidence/README.md`](evidence/README.md) — evidence taxonomy and retention policy.
- [`scripts/README.md`](scripts/README.md) — script taxonomy and entry points.

## License

See [`LICENSE`](LICENSE).
