# Phase 0 — Repository & Engineering Environment

## Status

**Completed** — milestone tag `v0.1-project-skeleton`.

Phase 0 establishes the repository, build system, test framework, static analysis, sanitizers, CI, and evidence workflow used by every later phase.

## Objective

Create a reproducible C++20 engineering baseline before adding product features. The phase proves that the project can be configured, built, tested, analyzed, and verified consistently on Ubuntu.

## Scope

Included in this phase:

- C++20 project configuration with CMake;
- reusable compiler-warning, sanitizer, and static-analysis modules;
- CMake presets for repeatable local builds;
- GoogleTest integration;
- `clang-format` and `clang-tidy` quality gates;
- AddressSanitizer and UndefinedBehaviorSanitizer;
- GitHub Actions for build, test, and static analysis;
- scripts for local build/test/verification;
- Phase 0 evidence collection.

Deliberately excluded:

- Linux process/IPC features;
- TCP/UDP networking;
- SIP, RTP, PJSIP, audio, or Qt/QML.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| Build system | Configure and build the C++20 project | `CMakeLists.txt`, `CMakePresets.json` |
| CMake modules | Centralize warnings, sanitizers, and static analysis | `cmake/` |
| CI | Re-run project quality gates on GitHub | `.github/workflows/` |
| Build helpers | Provide repeatable local commands | `scripts/dev/build.sh`, `scripts/dev/test.sh` |
| Verification | Run the Phase 0 quality gates | Phase 0 scripts and CI workflows |
| Evidence | Preserve verified toolchain/build/test output | `evidence/phase0/` |

## Architecture and design

The engineering pipeline established in this phase is:

```text
source
  |
  v
CMake configuration
  |
  +--> compiler warnings
  +--> clang-format
  +--> clang-tidy
  +--> ASan / UBSan
  |
  v
build
  |
  v
GoogleTest
  |
  v
CI + evidence
```

The important design rule is that quality checks are part of the project from the beginning instead of being added after the application becomes large.

## Build and run

A direct CMake build is supported:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The preferred development workflow uses the checked-in presets:

```bash
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug --output-on-failure
```

Project helper scripts provide the same workflow through repository-owned commands.

## Verification

Phase 0 verification covers:

- CMake configure and build;
- unit tests;
- formatting checks;
- `clang-tidy`;
- ASan and UBSan;
- GitHub Actions build/test/static-analysis workflows;
- `git diff --check` before committing changes.

A clean Phase 0 repository should be able to regenerate its build directory from source without relying on committed generated files.

## Evidence

Committed evidence lives under:

```text
evidence/phase0/
```

The directory records the verified toolchain and representative build, test, static-analysis, and sanitizer output from the Phase 0 environment.

## Interview focus

Be ready to explain:

- why CMake presets improve reproducibility;
- the difference between compiler warnings, static analysis, and sanitizers;
- what ASan and UBSan detect;
- why tests and quality gates belong in CI;
- why generated build artifacts are excluded from Git;
- how the project can be rebuilt from a clean working tree.

## Exit criteria

Phase 0 is complete when all of the following statements are true:

- the repository configures and builds from a clean source tree;
- unit tests pass locally and in CI;
- formatting and static analysis pass;
- sanitizer builds/tests pass;
- generated build output is not tracked;
- Phase 0 evidence has been collected and reviewed;
- milestone tag `v0.1-project-skeleton` points to the completed Phase 0 state.

## References

- Root project overview: `README.md`
- Project roadmap: `ROADMAP.md`
- Evidence: `evidence/phase0/`

## Next phase

Phase 1 applies the engineering baseline to Modern C++ ownership, value semantics, state modeling, interfaces, and unit-tested production code.
