# Linux VoIP Softphone

A Linux SIP softphone engineering portfolio developed incrementally in C++20 with Linux system programming, TCP/UDP networking, Asterisk, PJSIP/PJSUA2, testing, sanitizers, static analysis, and protocol-focused verification.

The repository is intentionally phase-driven: each milestone adds production code, tests, documentation, and verification evidence before moving to the next subsystem.

## Current status

Completed through **Phase 8 — Multithreading** for the documented development and CI environments. Phase 8 release identifier: `v0.9-threading` (reserved for an annotated tag after the Phase 8 branch is reviewed, merged into `main`, and the closure state is verified; this document does not assert that the tag is already published).

Implemented so far:

- C++20 project/toolchain foundation with CMake, GoogleTest, clang-format, clang-tidy, ASan, and UBSan.
- Modern C++ call-state core and focused ownership/value-semantics labs.
- Linux process, signal, file-descriptor RAII, and Unix-domain-socket IPC work.
- IPv4 endpoint, TCP client/server, and UDP socket abstractions with integration tests.
- Local Asterisk SIP lab with two endpoints and documented REGISTER/call flows.
- PJPROJECT 2.17 / PJSUA2 integration behind `ENABLE_PJSIP`.
- PJSIP account and endpoint configuration abstractions.
- CLI registration client.
- CLI outgoing-call lifecycle.
- CLI incoming-call lifecycle with ringing and auto-answer support.
- Automated PJSIP contract, regression, sanitizer, and static-analysis verification.
- Scrubbed runtime evidence that excludes SIP credentials and keying material.
- Optional Qt 6/QML GUI build with a QML -> ViewModel -> CallManager -> SipEngine boundary.
- Long-lived PJSUA2 GUI backend for registration plus one active outgoing or incoming call.
- GUI registration, dialer, incoming-call answer/hangup, active-call duration, and diagnostics screens.
- Deterministic CallManager/ViewModel tests and an offscreen GUI startup smoke test.
- Dedicated `WorkerExecutor` and `ThreadedSipEngine` boundary for serialized SIP backend work off the Qt UI thread.
- Worker-owned SIP backend construction, command execution, and destruction with deterministic shutdown/drain behavior.
- Reusable `ThreadSafeQueue<T>` with mutex/condition-variable close-and-drain semantics.
- 100,000-event four-producer/four-consumer stress verification with atomic exactly-once accounting.
- ThreadSanitizer build support plus required CI runtime coverage for Phase 8 concurrency tests.

Phase 6 established the reusable SIP/RTP/audio path, and Phase 7 added the Qt/QML presentation boundary. Phase 8 keeps that boundary intact while moving concrete SIP backend ownership and commands behind a dedicated worker executor, adding a reusable producer-consumer queue, lifecycle/stress verification, and ThreadSanitizer coverage. Curated Phase 8 verification summaries are retained under `evidence/phase8/`.

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

Detailed historical and future planning lives in [`ROADMAP.md`](ROADMAP.md).

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

Run the complete Phase 8 automated verification suite (which inherits the Phase 7 gate and adds threading/concurrency verification):

```bash
./scripts/phase8/verify.sh
```

## Qt/QML GUI application

For the real GUI SIP backend, configure both Qt and PJSIP:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake --preset qt-pjsip-debug
cmake --build --preset qt-pjsip-debug -j
./build/qt-pjsip-debug/src/gui/linux-voip-softphone-gui
```

The GUI keeps QML independent of PJSUA2 through `CallViewModel`, `CallManager`, and the `SipEngine` interface. Phase 7 architecture, verification, and evidence are documented in `docs/phase7/README.md`, `docs/architecture/qt-qml-gui.md`, and `evidence/phase7/`.

## PJSIP CLI applications

When `ENABLE_PJSIP=ON`, the project builds three CLI applications:

```text
voip-sip-register   one-shot SIP registration verification
voip-sip-call       register and place one outgoing call
voip-sip-listen     register and handle one incoming call lifecycle
```

Usage and architecture details are documented in:

```text
docs/phase5/README.md
docs/phase6/README.md
docs/architecture/pjsip-cli.md
docs/architecture/audio-rtp.md
```

SIP passwords are supplied through an environment variable rather than command-line arguments. Do not commit credentials, raw Digest Authorization headers, or SDP keying material.

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
v0.9-threading         Phase 8 — Multithreading (release identifier)
```

Phase 8 acceptance is complete for the documented development and CI environments based on the recorded automated, stress, lifecycle, and ThreadSanitizer checks. The annotated `v0.9-threading` tag is created only after this branch is reviewed, merged into `main`, and the milestone state is verified. Next planned phase: **Phase 9 — debugging, memory diagnostics, and core dumps**.

## Documentation

Useful entry points:

```text
docs/README.md                     documentation taxonomy and required schemas
ROADMAP.md                         complete project roadmap
docs/architecture/                component/system architecture and design rationale
docs/protocols/                   protocol wire/session behavior
docs/interview/                   phase review questions and answers
docs/phase*/README.md             primary phase entry points
evidence/                         scrubbed verification evidence by milestone
```

Architecture, protocol, interview, and primary phase README documents intentionally use category-specific common schemas. Run `./scripts/quality/verify-docs-structure.sh` to verify those conventions.

## License

See [`LICENSE`](LICENSE).
