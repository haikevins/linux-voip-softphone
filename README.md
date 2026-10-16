# Linux VoIP Softphone

A Linux SIP softphone engineering portfolio developed incrementally in C++20 with Linux system programming, TCP/UDP networking, Asterisk, PJSIP/PJSUA2, testing, sanitizers, static analysis, and protocol-focused verification.

The repository is intentionally phase-driven: each milestone adds production code, tests, documentation, and verification evidence before moving to the next subsystem.

## Current status

Completed through **Phase 7 — Qt/QML GUI** for the documented local lab. Phase 7 release identifier: `v0.8-qt-gui` (reserved for an annotated tag after the Phase 7 branch is reviewed, merged into `main`, and the closure state is verified; this document does not assert that the tag is already published).

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

Phase 6 established the reusable SIP/RTP/audio path. Phase 7 layers a Qt/QML presentation on top without calling PJSIP directly from QML. The documented local GUI acceptance reached SIP registration, outgoing and incoming calls, visible call-state transitions, clear two-way audio, and diagnostics. Curated screenshots and sanitized summaries are retained under `evidence/phase7/`.

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

Run the complete Phase 7 automated verification suite (which inherits the Phase 6/5 gates and adds Qt/QML verification):

```bash
./scripts/phase7/verify.sh
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
```

Phase 7 acceptance is complete for the documented local lab based on the recorded automated and live GUI tests. The annotated `v0.8-qt-gui` tag is created only after this branch is reviewed, merged into `main`, and the milestone state is verified. Next planned phase: **Phase 8 — multithreading**.

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
