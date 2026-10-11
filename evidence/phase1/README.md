# Phase 1 Evidence — Modern C++ Foundation

## Status

**Completed** — Phase 1 milestone tag `v0.2-cpp-core`.

## Verification scope

Evidence covers the C++20 core, call-state behavior, ownership/move-semantics labs, formatting, sanitizers, unit tests, static analysis, and captured toolchain.

## Environment

The retained `toolchain.txt` and logs record the original run. Current build requirements belong in `docs/phase1/README.md`.

## Automated verification

Run:

```bash
./scripts/phase1/verify.sh
```

The accepted Phase 1 gate recorded 11/11 normal tests and 11/11 sanitizer tests, plus formatting and clang-tidy checks.

## Manual verification

The Phase 1 labs are exercised locally by the verifier. No external SIP endpoint or manual telephony flow is required.

## Artifacts

- `toolchain.txt` — original compiler/toolchain information.
- `build.log` — build outcome.
- `unit-tests.log` — unit test outcomes.
- `sanitizers.log` — sanitizer outcomes.
- `format-check.log` — formatting check.
- `clang-tidy.log` — static-analysis check.
- `raii-demo.log` — RAII lab execution.
- `smart-pointers-demo.log` — smart-pointer lab execution.
- `move-semantics-demo.log` — move-semantics lab execution.

These are historical captured outcomes, not normative documentation.

## Results

Within the recorded verification scope, phase 1 verification passed and the Modern C++ core became the base for Phase 2.

## Security and redaction

No SIP credentials, authentication material, or secret-bearing packet traces belong in Phase 1 evidence.

## Reproduction

Run `./scripts/phase1/verify.sh`. Use `scripts/phase1/collect-evidence.sh` only when intentionally collecting a new evidence run; do not overwrite historical observations to make them appear current.

## References

- `docs/phase1/README.md`
- `docs/architecture/call-state-machine.md`
- `docs/interview/phase1-questions.md`
- Git tag `v0.2-cpp-core`
