# Phase 2 Evidence — Linux System Programming and IPC

## Status

Complete. Phase 2 established Linux process, signal, file-descriptor, and Unix-domain-socket foundations and the `v0.3-linux-system` milestone.

## Verification scope

Evidence covers RAII file-descriptor ownership, process and exec labs, signal handling, Unix-domain-socket unit/integration behavior, sanitizers, formatting, and static analysis.

## Environment

Historical toolchain snapshots retained in this directory are authoritative for the original run. Current build requirements are documented in `docs/phase2/README.md`.

## Automated verification

The Phase 2 verifier is:

```bash
./scripts/phase2/verify.sh
```

The recorded final Phase 2 gate passed 32/32 tests, the 2/2 IPC integration subset, sanitizer verification, process/signal labs, formatting, and clang-tidy.

## Manual verification

No external telephony endpoint is required. Process and IPC behavior is reproduced locally by the phase verifier and labs.

## Artifacts

Historical artifacts include build/test summaries, sanitizer output, integration results, process/signal lab output, toolchain information, and Git-state snapshots. These files are retained because they document the completed Phase 2 verification run.

## Results

Phase 2 verification passed and the Linux systems layer became the stable base for networking work.

## Security and redaction

No SIP credentials or secret-bearing packet traces belong in this phase.

## Reproduction

Run:

```bash
./scripts/phase2/verify.sh
```

Use `scripts/phase2/collect-evidence.sh` only when intentionally refreshing Phase 2 evidence; its destination is `evidence/phase2/`.

## References

- `docs/phase2/README.md`
- `docs/architecture/linux-process-ipc.md`
- `docs/interview/phase2-questions.md`
- Git tag `v0.3-linux-system`
