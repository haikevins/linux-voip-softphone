# Phase 1 Evidence — Modern C++ Foundation

## Status

Complete. Phase 1 established the Modern C++ core and the `v0.2-cpp-core` milestone.

## Verification scope

Evidence covers the C++20 core, call state machine behavior, ownership and move-semantics labs, formatting, sanitizer execution, tests, and static analysis.

## Environment

Historical toolchain snapshots retained in this directory are authoritative for the original run. Current build requirements are documented in `docs/phase1/README.md`.

## Automated verification

The Phase 1 verifier is:

```bash
./scripts/phase1/verify.sh
```

The recorded Phase 1 gate passed, including 11/11 tests in the normal test run and 11/11 under sanitizers, plus formatting and clang-tidy gates.

## Manual verification

The Phase 1 C++ labs are exercised by the verifier. No external SIP endpoint or manual telephony flow is required.

## Artifacts

Existing committed Phase 1 logs and summaries remain historical evidence. They should be treated as captured outcomes, not as normative documentation.

## Results

Phase 1 verification passed and the C++ core became the stable base for Linux systems work.

## Security and redaction

No SIP credentials or secret-bearing packet traces belong in this phase.

## Reproduction

Run:

```bash
./scripts/phase1/verify.sh
```

Use `scripts/phase1/collect-evidence.sh` only when intentionally refreshing Phase 1 evidence; its destination is `evidence/phase1/`.

## References

- `docs/phase1/README.md`
- `docs/architecture/call-state-machine.md`
- `docs/interview/phase1-questions.md`
- Git tag `v0.2-cpp-core`
