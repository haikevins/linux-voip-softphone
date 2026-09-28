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
pkg-config --static --libs libpjproject
```

The default PJPROJECT POSIX build produces static archives. Dependencies such as OpenSSL, ALSA,
SRTP, and bundled codec libraries are recorded in `Libs.private` in `libpjproject.pc`; static
consumers therefore must use pkg-config's `--static` metadata. The standalone verifier and the
`PJSIP::PJSIP` CMake target both follow that rule.

## Ubuntu 24.04 note

Ubuntu 24.04 does not provide the `pjproject` development package used by this Phase 5 baseline in
the configured workstation repositories. The workstation therefore uses an upstream/local
PJPROJECT installation that exports `libpjproject.pc`.

The project baseline for Phase 5 is PJSIP `2.17` or newer. For the versioned local installation used
by this lab, make its pkg-config metadata visible before configuring the project:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
```

A default static PJPROJECT installation does not require an `LD_LIBRARY_PATH` override. If a shared
PJPROJECT build is used instead, ensure its runtime libraries are discoverable by the dynamic
linker.

## Verify the dependency

Run the standalone dependency verifier first:

```bash
./scripts/verify_pjsip_dependency.sh
```

It checks:

- `pkg-config` is available;
- `libpjproject >= 2.17` is discoverable;
- the C++ compiler can include `<pjsua2.hpp>`;
- a minimal PJSUA2 program links and runs successfully with the static transitive dependencies.

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

The normal `debug`, `sanitizers`, and `clang-tidy` presets remain unchanged in this commit. Later
Phase 5 commits will add the PJSIP-backed CLI target and extend the quality presets around it.
