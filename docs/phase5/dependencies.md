# Phase 5 — PJSIP Dependency Provisioning

## Purpose

Document the Phase 5 build-time dependency contract for PJPROJECT/PJSIP/PJSUA2 without mixing dependency provisioning into the PJSIP client architecture document.

PJSIP support is opt-in so the existing non-PJSIP presets remain useful regression paths while the dedicated PJSIP preset requires the dependency explicitly.

## Dependency contract

Configure the PJSIP-enabled build with:

```bash
cmake --preset pjsip-debug
```

When PJSIP is enabled, CMake requires the `libpjproject` pkg-config module at version `2.17` or newer and exposes the dependency through the project CMake integration used by the PJSIP targets.

## Why pkg-config

PJPROJECT installs `libpjproject.pc`, which carries include directories, compile definitions, library search paths, and transitive link dependencies. The project consumes that metadata through CMake's `FindPkgConfig` path instead of hard-coding architecture-specific library names.

Useful inspection commands are:

```bash
pkg-config --modversion libpjproject
pkg-config --cflags libpjproject
pkg-config --libs libpjproject
pkg-config --static --libs libpjproject
```

The `--static` form matters when the local PJPROJECT installation provides static libraries and the consumer must include transitive link dependencies.

## Ubuntu 24.04 baseline

The Phase 5 workstation uses a local PJPROJECT 2.17 installation under a user-local prefix. Expose its pkg-config metadata before configuring:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
```

The dependency verifier and CMake configure step should discover the same installation. Avoid mixing headers from one PJPROJECT installation with libraries/pkg-config metadata from another.

If a future installation uses shared libraries under a non-system prefix, configure the system runtime linker or an appropriate development-session library path. The current project baseline does not require documentation to hard-code a shared-library path when the selected build is static.

## Verify the dependency

Run the standalone dependency verifier first:

```bash
./scripts/verify_pjsip_dependency.sh
```

It checks that:

- `pkg-config` is available;
- `libpjproject >= 2.17` is discoverable;
- the C++ compiler can include `<pjsua2.hpp>`;
- a minimal PJSUA2 program can compile, link, and run.

Expected final lines:

```text
PJSUA2 compile/link smoke test: PASS
PJSIP dependency verification: PASS
```

## CMake verification paths

Exercise the PJSIP-enabled configure/build path:

```bash
cmake --preset pjsip-debug
cmake --build --preset pjsip-debug -j
```

A successful configure reports the discovered PJPROJECT version and enables the PJSIP targets.

Phase 5 also verifies PJSIP-enabled sanitizer and static-analysis paths while preserving the normal non-PJSIP regression path. The completed workflow is orchestrated by:

```bash
./scripts/verify_phase5.sh
```

## Phase boundary

This document owns dependency discovery/provisioning details only. PJSUA2 object ownership, callbacks, call/account lifecycle, and shutdown ordering belong in `docs/architecture/pjsip-cli.md`. CLI usage and phase-level verification belong in `docs/phase5/README.md`.

## References

- `docs/phase5/README.md`
- `docs/architecture/pjsip-cli.md`
- `scripts/verify_pjsip_dependency.sh`
- `scripts/verify_phase5.sh`
