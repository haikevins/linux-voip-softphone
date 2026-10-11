# Linux VoIP Softphone

A Linux SIP softphone engineering portfolio developed incrementally in C++20 with Linux system programming, TCP/UDP networking, Asterisk, PJSIP/PJSUA2, Qt/QML, testing, sanitizers, static analysis, and protocol-focused verification.

The repository is intentionally phase-driven: each milestone adds production code, tests, documentation, and verification evidence before moving to the next subsystem.

## Current status

Phase 7 — Qt/QML GUI is accepted for the documented local lab and published as annotated tag `v0.8-qt-gui`. Phase 8 — Multithreading has been merged to `main` and post-merge automated verification passes; final maintainer Phase 8 acceptance remains pending, with the documented PJSUA2 shutdown boundaries still **BLOCKED/UNPROVEN**.

Implemented so far:

- C++20 project/toolchain foundation with CMake, GoogleTest, clang-format, clang-tidy, ASan, and UBSan.
- Modern C++ call-state core and focused ownership/value-semantics labs.
- Linux process, signal, file-descriptor RAII, and Unix-domain-socket IPC work.
- IPv4 endpoint, TCP client/server, and UDP socket abstractions with integration tests.
- Local Asterisk SIP lab with two endpoints and documented REGISTER/call flows.
- PJPROJECT 2.17 / PJSUA2 integration behind `ENABLE_PJSIP`.
- CLI registration plus outgoing/incoming call lifecycles.
- Real Linux two-way audio with RTP/RTCP diagnostics and Phase 6 wire-level evidence.
- Optional Qt/QML GUI shell that remains network-free when PJSIP is disabled.
- Qt application layer with `GuiViewModel`, Qt-free `CallManager`, and a retained PJSUA2 worker/session.
- Live SIP registration, outgoing calls, incoming calls, Answer/Decline, and local/remote hangup handling.
- Explicit SIP `603 Decline` on the softphone-to-Asterisk call leg.
- Connected-call duration with deterministic reset at teardown.
- Read-only live RTP/audio diagnostics in QML: codec, RX/TX packets, loss, jitter, and RTCP RTT.
- Registration reuse across sequential incoming/outgoing calls without re-registering.
- Regression verification that keeps the Phase 5/6 signaling and media behavior green.
- Phase 8 `ThreadSafeQueue<T>` and owned `SerialTaskWorker` concurrency primitives with focused unit/integration coverage.
- Repeated 100,000-event MPMC stress verification and standalone Clang 18 ThreadSanitizer coverage.
- Hardened Qt/PJSUA2 ownership, callback-lifetime, and shutdown-order contracts with explicit fail-closed behavior where safe teardown cannot be proven.

Phase 7 intentionally excluded multiple accounts, call waiting, mute/hold, audio-device selection UI, persistent call history, and a general-purpose cross-thread event queue from its GUI acceptance scope. Phase 8 subsequently added the reusable queue/worker and stronger lifetime/shutdown contracts without retroactively expanding the Phase 7 milestone. Multiple accounts, call waiting, mute/hold, audio-device selection UI, and persistent call history remain future work.

## Repository layout

```text
cmake/          reusable CMake modules and quality configuration
config/         example runtime configuration, including the Asterisk lab
docs/           documentation taxonomy, architecture, protocols, interviews, and phase guides
evidence/       committed, scrubbed verification evidence by milestone
labs/           focused learning/demo programs for core concepts
scripts/        build, test, static-analysis, and phase verification helpers
src/            production C++ code
tests/          unit and integration tests
```

Detailed historical and future planning lives in [`README_roadmap.md`](README_roadmap.md).

## Build the non-PJSIP targets

```bash
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug --output-on-failure
```

The base build does not require PJPROJECT.

## Build with PJSIP

PJPROJECT 2.17 is installed locally in the development environment. Point `pkg-config` at the installation first:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
```

Then configure and build the PJSIP preset:

```bash
cmake --preset pjsip-debug
cmake --build --preset pjsip-debug -j
```

Verify the dependency independently:

```bash
./scripts/phase5/verify-pjsip-dependency.sh
```

Run the complete inherited media gate:

```bash
./scripts/phase6/verify.sh
```

## PJSIP CLI applications

When `ENABLE_PJSIP=ON`, the project builds three CLI applications:

```text
voip-sip-register   one-shot SIP registration verification
voip-sip-call       register and place one outgoing call
voip-sip-listen     register and handle one incoming call lifecycle
```

CLI/media details remain documented in:

```text
docs/phase5/README.md
docs/phase6/README.md
docs/architecture/pjsip-cli.md
docs/architecture/audio-rtp.md
```

## Build the Qt GUI

The Qt-only preset builds the QML shell without live SIP networking:

```bash
cmake --preset qt-gui-debug
cmake --build --preset qt-gui-debug --clean-first -j2
```

The combined production GUI enables Qt and PJSIP together:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake --preset qt-pjsip-gui-debug
cmake --build --preset qt-pjsip-gui-debug --clean-first -j2
```

Run the combined GUI with the registered extension and SIP server using `--sip-register USER SERVER [PORT]`. Supply the SIP password through the existing hidden stdin flow; do not put real credentials in argv, source files, evidence, screenshots, or committed logs.

Phase 7 architecture and verification details are documented in:

```text
docs/phase7/README.md
docs/architecture/qt-qml.md
docs/interview/phase7-questions.md
evidence/phase7/README.md
```

## Verification philosophy

A phase is complete only when the repository contains all four layers:

```text
source code
    ↓
automated tests
    ↓
runtime evidence
    ↓
written explanation
```

The current repository therefore keeps historical `docs/`, `labs/`, `scripts/`, and `evidence/` artifacts on purpose. They are part of the engineering record, not generated build output.

Generated build trees are excluded from Git and can be recreated at any time.

## Milestones

```text
v0.1-project-skeleton  Phase 0 — repository and engineering environment
v0.2-cpp-core          Phase 1 — modern C++ foundation
v0.3-linux-system      Phase 2 — Linux system programming and IPC
v0.4-networking        Phase 3 — TCP/UDP networking
v0.5-sip-asterisk      Phase 4 — SIP + Asterisk lab
v0.6-pjsip-cli         Phase 5 — PJSIP CLI softphone
v0.7-rtp-audio         Phase 6 — RTP + Audio
v0.8-qt-gui            Phase 7 — Qt/QML GUI (release identifier)
```

Phase 7 acceptance is complete and `v0.8-qt-gui` is published. Phase 8 — Multithreading is merged to `main` and post-merge automated verification passes; final maintainer acceptance is still pending. After that explicit acceptance, the next planned phase is **Phase 9 — Debugging, memory and core dump**.

## Documentation

Useful entry points:

```text
docs/README.md                     documentation taxonomy and required schemas
README_roadmap.md                  complete project roadmap
docs/architecture/                component/system architecture and design rationale
docs/protocols/                   protocol wire/session behavior
docs/interview/                   phase review questions and answers
docs/phase*/README.md             primary phase entry points
evidence/                         scrubbed verification evidence by milestone
```

Architecture, protocol, interview, and primary phase README documents intentionally use category-specific common schemas. Run `./scripts/quality/verify-docs-structure.sh` to verify those conventions.

## License

See [`LICENSE`](LICENSE).
