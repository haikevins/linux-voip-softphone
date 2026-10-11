# Phase 8 Historical Evidence

## Purpose

`history/` contains Phase 8 per-commit/checkpoint evidence that remains useful for provenance but is not part of the current/core evidence surface.

These files were moved from the Phase 8 root only to reduce clutter. Their contents are preserved byte-for-byte; the move does not change the historical result or upgrade any acceptance claim.

## Contents

Concurrency and ownership checkpoints:

- `callback-activity.txt`
- `ownership-audit.txt`
- `ownership-source-review.txt`
- `pjsip-callback-ingress-races.txt`

Shutdown/lifetime checkpoints:

- `pjsip-cleanup-timeout.txt`
- `pjsip-production-lifetime-audit.txt`
- `pjsip-shutdown-authorization.txt`
- `pjsip-shutdown-failure-paths.txt`
- `pjsip-shutdown-integration-contract.txt`
- `pjsip-shutdown-order.txt`
- `pjsip-shutdown-ownership-policy.txt`

Build/runtime corrective checkpoints:

- `ci-clang-format-hotfix.txt`
- `clang-tidy-regression-hotfix.txt`
- `final-runtime-acceptance-6b19d5e.txt`

## Provenance

Each file corresponds to a distinct Phase 8 development checkpoint or corrective verification run. Similar subject matter across files does not make them duplicates: the recorded revision, condition, or verification purpose differs.

The relocation into `history/` preserves each file's bytes and SHA256 value. Git history retains the original root path and the later organizational move.

## Retention

Keep these files while they provide useful provenance for Phase 8 decisions, regressions, or shutdown/lifetime investigations. Do not merge them into a synthetic combined artifact unless the original per-checkpoint evidence remains independently recoverable and the repository's verification references are updated deliberately.

Current/core and roadmap evidence remains at `evidence/phase8/`.

## Security and redaction

The same evidence policy applies here: do not add SIP passwords, raw Digest Authorization material, SRTP inline keys, sensitive packet captures, core dumps, or unrelated generated output.

## References

- `../README.md`
- `../../README.md`
- `../../../docs/phase8/README.md`
- `../../../docs/architecture/threading-model.md`
- `../../../docs/phase8/roadmap-exit-review.md`
