# Phase 5 — PJSIP Dependency Setup

## Goal

Commit 1 introduces explicit build-time detection for the PJSIP/PJSUA2 development dependency that
will back the Phase 5 CLI softphone.

PJSIP support is opt-in for now so existing project presets remain usable while the dependency is
being provisioned. Configure the dedicated preset to require PJSIP:

```bash
cmake --preset pjsip-debug
```

When enabled, CMake requires the `libpjproject` pkg-config module at version `2.17` or newer and
publishes it as the CMake target `PJSIP::PJSIP` for later Phase 5 targets.

## Why pkg-config

PJPROJECT installs `libpjproject.pc`, which carries the include directories, compile definitions,
library search paths, and transitive libraries needed by PJSIP/PJSUA2. The build consumes that
metadata through CMake's `FindPkgConfig` module instead of hard-coding architecture-specific library
names.

Useful inspection commands:

```bash
pkg-config --modversion libpjproject
pkg-config --cflags libpjproject
pkg-config --libs libpjproject
```

## Ubuntu 24.04 note

Ubuntu 24.04 does not provide the `pjproject` source package in the standard Noble release archive.
The Phase 5 workstation therefore needs an upstream/local PJPROJECT installation that exports
`libpjproject.pc`.

The project baseline for Phase 5 is PJSIP `2.17` or newer. If PJPROJECT is installed under a custom
prefix, make its pkg-config metadata visible before configuring the project. For example:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
```

If shared libraries were installed under the same private prefix, the runtime linker also needs to
find them. Prefer configuring the system linker for a persistent installation; for a temporary lab
session, an environment override can be used:

```bash
export LD_LIBRARY_PATH="$HOME/.local/pjproject/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

## Verify the dependency

Run the standalone dependency verifier first:

```bash
./scripts/verify_pjsip_dependency.sh
```

It checks:

- `pkg-config` is available;
- `libpjproject >= 2.17` is discoverable;
- the C++ compiler can include `<pjsua2.hpp>`;
- a minimal PJSUA2 program links and runs successfully.

Expected final lines:

```text
PJSUA2 compile/link smoke test: PASS
PJSIP dependency verification: PASS
```

Then exercise the CMake detection path:

```bash
cmake --preset pjsip-debug
cmake --build --preset pjsip-debug -j
```

A successful configure prints a line similar to:

```text
-- PJSIP support enabled: libpjproject 2.17
```

The normal `debug` preset remains a non-PJSIP regression path. Phase 5 enables PJSIP explicitly
for sanitizer and clang-tidy verification when required:

```bash
cmake --preset sanitizers -DENABLE_PJSIP=ON
cmake --build --preset sanitizers -j
ctest --preset sanitizers --output-on-failure

cmake --preset clang-tidy -DENABLE_PJSIP=ON
cmake --build --preset clang-tidy -j
```

The completed Phase 5 workflow is available through `./scripts/verify_phase5.sh`. It verifies the
PJSIP dependency, PJSIP-enabled build/tests, sanitizers, clang-tidy, CLI integration contract, and
the non-PJSIP regression path.
