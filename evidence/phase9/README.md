# Phase 9 Evidence — Debugging, Memory Diagnostics, and Core Dumps

## Status

Completed. Phase 9 established the debugging/memory-diagnostics workflow and the `v0.9-diagnostics` milestone.

## Verification scope

Evidence covers the lab-only controlled crash generator, deterministic saved-core analysis with GDB, the historical heap use-after-free lesson, the repaired object-lifetime path, ASan+UBSan verification, safe Phase 9 CTest regression, the inherited Phase 8 verification gate, and the required branch CI result.

## Environment

Verification used the documented Linux development environment and the repository CMake presets. The inherited Qt/PJSIP gate uses the existing PJPROJECT 2.17 development installation exposed through `PKG_CONFIG_PATH`. GDB/core and sanitizer working files are generated only under disposable `build/` directories.

## Automated verification

Run:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase9/verify.sh
```

The accepted local closure preparation completed with `Phase 9 automated verification: PASS`. The safe Phase 9 CTest label reported 2/2 passing tests. A full PJSIP debug rerun reported 141/141 passing tests, and `Phase9RepairedLifetimeRegressionTest` was repeated 20 times without failure. The Phase 9 Commit 6 branch CI completed 4/4 required jobs successfully. See `automated-verification.txt`.

## Manual verification

No separate human-operated SIP/audio acceptance is required by this debugging milestone. Phase 9 intentionally exercises controlled lab faults and automated diagnostic workflows; live SIP/audio behavior remains covered by the earlier completed milestones.

## Artifacts

- `automated-verification.txt` — concise local regression and CI summary.
- `gdb-core-analysis.txt` — reviewed saved-core/GDB signal and backtrace contract.
- `asan-report.txt` — historical UAF root cause plus repaired ASan/UBSan result.

These are curated summaries rather than raw terminal transcripts, core files, or sanitizer crash dumps.

## Results

The controlled null-dereference workflow produced the expected `SIGSEGV` under GDB, generated a saved core, and recovered the required `triggerNullDereference` and `main` backtrace markers in a fresh GDB session. The repaired lifetime path completed without accessing freed memory and passed fresh ASan+UBSan verification. Safe Phase 9 CTest regression passed locally, including repeated execution of the lifetime regression, and the accepted Commit 6 branch CI completed 4/4 required jobs.

These results validate the documented workflows for the tested revision and environments; they do not prove that every possible production memory defect is absent.

## Security and redaction

The retained summaries contain no core file, raw GDB dump, raw sanitizer crash transcript, SIP password, Digest Authorization material, SRTP key material, or packet capture. Diagnostic binaries and working logs remain disposable under `build/`.

## Reproduction

Run the complete Phase 9 verifier:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase9/verify.sh
```

Run the two focused diagnostic workflows directly with:

```bash
./scripts/phase9/capture-core-dump.sh
./scripts/phase9/verify-asan-use-after-free.sh
```

The safe Phase 9 CTest regression can be inspected with:

```bash
ctest --test-dir build/pjsip-debug -L phase9 --output-on-failure
```

## References

- Phase guide: `docs/phase9/README.md`
- Architecture: `docs/architecture/debugging-diagnostics.md`
- Core/lifetime analysis: `docs/phase9/core-dump-analysis.md`
- Interview review: `docs/interview/phase9-questions.md`
- Aggregate verifier: `scripts/phase9/verify.sh`
- Core workflow: `scripts/phase9/capture-core-dump.sh`
- ASan verifier: `scripts/phase9/verify-asan-use-after-free.sh`
- Crash generator: `tools/crash_generator/main.cpp`
