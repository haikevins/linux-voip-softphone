# Linux VoIP Softphone

A Linux/C++20 engineering project that grows from systems-programming foundations into a small SIP softphone with PJSIP/PJSUA2, RTP/audio, Qt/QML, threading/lifetime hardening, diagnostics, and an RTSP/RTP mini-lab.

The repository is organized as a sequence of reviewed milestones. Each phase keeps implementation, verification, documentation, and evidence together so design claims can be traced back to code and observed behavior.

## Current status

Phases 0–10 are accepted at their published milestones through `v0.10-rtsp-lab`.

Phase 11 — Final Engineering Release is **in progress** on `phase/11-final-release`. The current phase adds reproducible Release packaging, packaged-system acceptance, final architecture/operations documentation, and portfolio/demo presentation. The target release is `v1.0.0`, but that tag is intentionally **not** created until the phase is closed, merged to `main`, and the full verification gate passes again on synchronized `main`.

The final-release work does not silently expand product scope. It packages and presents the already reviewed SIP, media, GUI, concurrency, diagnostics, and RTSP work.

## What the project demonstrates

- C++20 ownership, RAII, state modeling, and testable application boundaries.
- Linux process, signal, IPC, TCP, UDP, and socket lifecycle work.
- SIP registration and call flows against a local Asterisk lab.
- PJPROJECT/PJSIP 2.17 integration through PJSUA2.
- CLI registration, outgoing-call, and incoming-call applications.
- Linux capture/playback with RTP/RTCP diagnostics and two-way-audio lab evidence.
- Qt/QML presentation separated from application and PJSIP ownership.
- Worker-thread/event-handoff design, stress testing, sanitizer coverage, and explicit shutdown/lifetime boundaries.
- Controlled crash/core-dump/GDB diagnostics and repaired lifetime regression coverage.
- RTSP/1.0 request/response parsing, SDP control discovery, UDP RTP reception, and loopback integration verification.
- Reproducible Release build/install/package flow plus acceptance of the extracted package.

This is an engineering/learning repository, not a claim of production certification across arbitrary SIP providers, Linux distributions, networks, or audio devices.

## Architecture at a glance

```text
Qt/QML presentation
        ↓
application state / call control
        ↓
threaded SIP engine boundary
        ↓
PJSUA2 / SIP signaling
        ↓
RTP / Linux audio

separate protocol-learning path:
RTSP client → TCP RTSP control → SDP → UDP RTP receiver

cross-cutting:
ownership / shutdown policy
sanitizers / diagnostics / regression gates
CMake presets / install / CPack / CI
```

The RTSP mini-lab is intentionally separate from the SIP softphone runtime. See [`docs/architecture/system-overview.md`](docs/architecture/system-overview.md) for the final cross-phase architecture map.

## Requirements

The verified development environment uses:

- Linux;
- a C++20 compiler;
- CMake + Ninja;
- GoogleTest;
- clang-format and clang-tidy;
- Qt 6/QML for the GUI;
- PJPROJECT/PJSIP 2.17 for SIP/media integration.

For the locally installed PJSIP 2.17 prefix used by the project:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
```

## Build and verify

For the accumulated engineering regression path:

```bash
cmake --preset debug
cmake --build --preset debug -j"$(nproc)"
ctest --preset debug --output-on-failure
```

For the current Phase 11 aggregate gate:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase11/verify.sh
```

That gate inherits the earlier phase verification chain and adds final-release checks. A successful run is evidence for the tested revision/environment; live SIP credentials, external peers, and audible hardware behavior remain distinct manual/runtime concerns where documented.

## Release build and package

Phase 11 adds a Release preset with Qt/QML and PJSIP enabled:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

cmake --preset release
cmake --build --preset release -j"$(nproc)"
./scripts/phase11/verify-package.sh
```

The TGZ package is produced under:

```text
build/phase11-package/
```

The final package inventory includes:

```text
linux-voip-softphone
linux-voip-softphone-gui
rtsp-lab
voip-sip-register
voip-sip-call
voip-sip-listen
```

Package-level acceptance is run with:

```bash
./scripts/phase11/verify-final-acceptance.sh
```

This extracts the generated archive, checks archive/path safety and runtime dependencies, exercises packaged CLI entry points, and performs a bounded offscreen Qt GUI startup smoke test.

## Demo path

The recommended recruiter/interview demo is documented in [`docs/phase11/demo-guide.md`](docs/phase11/demo-guide.md).

A compact sequence is:

```text
1. Show milestone/tag history and repository structure.
2. Explain the Qt → application → threaded SIP engine → PJSUA2/RTP boundary.
3. Run the final automated/package acceptance path.
4. Demonstrate the GUI and, when the lab is available, one controlled SIP call.
5. Show RTP/audio diagnostics and the RTSP loopback mini-lab.
6. Finish with sanitizer/diagnostic/evidence structure and the release boundary.
```

Do not expose SIP passwords, Digest authorization material, private packet captures, SRTP keying material, or unrelated peer information during a demo.

## Repository layout

```text
cmake/          CMake helpers and dependency configuration
src/            production/application sources
labs/           focused learning/system-programming labs
tests/          unit, integration, stress, Qt, and regression tests
tools/          controlled diagnostic tools
scripts/        setup, quality, per-phase, and release verification
docs/           architecture, phase, protocol, and interview documentation
evidence/       sanitized verification records by phase
```

The repository keeps `labs/` separate from `src/`: labs prove focused concepts, while production-facing behavior belongs under `src/` with reusable tests and documented ownership/lifecycle rules.

## Milestones

```text
v0.1-project-skeleton
v0.2-cpp-core
v0.3-linux-system
v0.4-networking
v0.5-sip-asterisk
v0.6-pjsip-cli
v0.7-rtp-audio
v0.8-qt-gui
v0.9-diagnostics
v0.10-rtsp-lab
v1.0.0                 target after Phase 11 closure/merge/post-merge verification
```

`v1.0.0` must identify the exact verified `main` commit. It is not published from the phase branch merely because the Release package builds.

## Documentation and evidence

Start with:

- [`README_roadmap.md`](README_roadmap.md) — phase-by-phase engineering roadmap;
- [`docs/phase11/README.md`](docs/phase11/README.md) — current final-release phase contract;
- [`docs/architecture/system-overview.md`](docs/architecture/system-overview.md) — final architecture/runtime map;
- [`docs/phase11/demo-guide.md`](docs/phase11/demo-guide.md) — final demo/reviewer walkthrough;
- [`docs/interview/phase11-questions.md`](docs/interview/phase11-questions.md) — final engineering-release review;
- [`evidence/`](evidence/) — sanitized historical evidence for accepted milestones.

Phase 11 closure evidence is added in the final closure commit. Until that commit and the post-merge gate complete, current-facing Phase 11 documentation must continue to describe `v1.0.0` as a target rather than a published release.

## Release boundary

The current `v1.0.0` scope is the documented engineering release of the existing project. It does not add or promise multiple simultaneous calls/accounts, conferencing, hold/mute, new codecs, SRTP/TLS deployment hardening, RTSP decode/playback inside the SIP GUI, or universal deployment support.

Future feature work should begin from an explicit new roadmap decision after the final release rather than being appended silently to Phase 11.
