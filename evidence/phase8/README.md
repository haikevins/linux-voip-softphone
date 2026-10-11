# Phase 8 Evidence — Multithreading

## Status

**PENDING** — Phase 8 automated evidence passes within its stated scope, but final maintainer acceptance has not been recorded.

Controlled PJSUA2 `PJ_EBUSY` recovery is recorded as observed/evidenced. Live external callback quiescence and production-client real `PJ_EBUSY` recovery remain **UNPROVEN**; PJSUA2 integration readiness remains **BLOCKED**.

## Verification scope

Retained evidence covers:

- historical preview-mode manual GUI acceptance;
- the controlled live PJSUA2 lifecycle observation;
- repeated 100,000-event MPMC stress;
- Clang 18 ThreadSanitizer verification.

Normal unit/integration/source-contract results are reproducible from the current tests and verification scripts and do not need separate per-commit evidence files.

## Environment

Recorded runs used the maintainer Linux/ThinkPad environment, Qt 6, PJPROJECT/PJSIP 2.17 where applicable, and Clang 18 for the successful TSan run.

Exact historical environment details needed to interpret an observation remain in the retained artifact itself.

## Automated verification

Current offline entry point:

```bash
bash scripts/phase8/verify.sh
```

Concurrency reproduction:

```bash
bash scripts/phase8/verify-threading.sh core
THREADING_STRESS_REPEATS=100 bash scripts/phase8/verify-threading.sh stress
CXX="$(command -v clang++-18)" THREADING_TSAN_REPEATS=3 \
    bash scripts/phase8/verify-threading.sh tsan
```

The recorded final canonical gate was 147/147 PASS. The recorded Clang 18 TSan gate was 18/18 PASS (six selected tests, three runs each).

The P8-R1 shutdown-attempt injection test is a new deterministic source/test
change. Its local standalone result is not an updated historical canonical
CTest count, live SIP observation, or production-client real `PJ_EBUSY` evidence.
When merged and reproduced on the maintainer host, the new test result is
reported separately from the retained historical artifacts.


## Manual verification

`manual-verification.txt` records the historical preview-mode GUI interaction/window-close acceptance.

`pjsip-live-lifecycle.txt` records a separate controlled PJSUA2 lifecycle run. It observed `PJ_EBUSY`, successful retry after hangup/call-slot cleanup, and no late registration callback in the recorded observation window.

Neither result proves universal external callback quiescence.

## Artifacts

- `manual-verification.txt` — historical GUI usability/window-close observation.
- `pjsip-live-lifecycle.txt` — controlled live PJSUA2 lifecycle observation.
- `stress-test.txt` — 100,000-event MPMC stress summary.
- `tsan-report.txt` — ThreadSanitizer summary and scope.

Per-commit closure/hotfix logs are intentionally left to Git history rather than retained in the current evidence tree.

## Results

- canonical automated verification: PASS within recorded scope;
- MPMC stress: PASS;
- Clang 18 TSan: PASS;
- historical preview-mode GUI acceptance: PASS;
- controlled live PJSUA2 `PJ_EBUSY` recovery: OBSERVED/EVIDENCED;
- live external callback quiescence: **UNPROVEN**;
- production-client real `PJ_EBUSY` recovery: **UNPROVEN**;
- PJSUA2 integration readiness: **BLOCKED**;
- final maintainer acceptance: **PENDING**.

## Security and redaction

Do not commit SIP passwords, Digest Authorization responses/nonces/cnonce values, SRTP inline keys, raw sensitive SIP traces, protected packet captures, raw audio, or unrelated generated build output.

The live lifecycle summary retains only sanitized facts and the hash of its temporary sanitized log.

## Reproduction

Run `bash scripts/phase8/verify.sh` for the offline gate.

Use `scripts/phase8/verify-threading.sh` for focused concurrency/stress/TSan reproduction. The live lifecycle probe remains explicit opt-in:

```bash
PHASE8_LIVE_SIP=1 bash scripts/phase8/run-pjsip-live-lifecycle-probe.sh
```

A new run is a new observation; do not rewrite historical results to appear current.

## References

- `docs/phase8/README.md`
- `docs/architecture/threading-model.md`
- `docs/interview/phase8-questions.md`
- `scripts/phase8/`
- `tests/unit/`
- `tests/integration/`
