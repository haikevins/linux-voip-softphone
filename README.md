# Linux VoIP Softphone

A Linux desktop SIP softphone engineering portfolio developed progressively with C++20,
Qt/QML, PJSIP, Linux system programming, networking, testing, and debugging tooling.

## Current milestone

**Phase 0 — Repository & Engineering Environment**

Target status after you complete the phase on your machine:

```text
Build: Passing
Tests: Passing
Compiler: GCC / Clang
Standard: C++20
Platform: Ubuntu
```

At this stage the repository intentionally does **not** implement VoIP features yet. The goal is
to establish a reproducible engineering baseline first.

## Phase 0 toolchain

```text
Git
CMake
C++20
clang-format
clang-tidy
GoogleTest
ASan
UBSan
GitHub Actions
```

Build pipeline:

```text
source
   ↓
CMake
   ↓
compile
   ↓
unit tests
   ↓
clang-tidy
   ↓
sanitizers
```

## Start here

Follow the Phase 0 tutorial in:

```text
docs/phase0/README.md
```

The full project roadmap from the planning session is preserved in:

```text
README_roadmap.md
```

## Required roadmap build commands

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build build -j

ctest --test-dir build
```

A preset-based workflow is also available:

```bash
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug
```

Run the starter executable:

```bash
./build/src/linux-voip-softphone
```

or, when using presets:

```bash
./build/debug/src/linux-voip-softphone
```

## Useful scripts

```bash
./scripts/bootstrap.sh
./scripts/build.sh
./scripts/test.sh
./scripts/run_clang_tidy.sh
./scripts/run_sanitizers.sh
./scripts/collect_phase0_evidence.sh
```

Read each script before running it. The scripts are conveniences, not substitutes for
understanding the commands.

## Phase 0 repository evidence

A recruiter should be able to immediately find:

```text
CMakeLists.txt
.clang-format
.clang-tidy
tests/
.github/workflows/
```

When the phase is genuinely complete, create the milestone tag:

```bash
git tag -a v0.1-project-skeleton \
    -m "Phase 0: repository and engineering environment"
```

## Project status

Phase 0 starter: **Ready for you to build and verify on Ubuntu.**
