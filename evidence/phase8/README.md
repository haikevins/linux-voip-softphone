# Phase 8 Evidence — Multithreading

## Status

**PENDING** — Phase 8 automated evidence passes within its stated scope, but final maintainer acceptance has not been recorded.

Controlled PJSUA2 `PJ_EBUSY` recovery and one bounded P8-R2B early-shutdown
recovery on a production-owned outgoing Account are **OBSERVED/EVIDENCED**.
Other production-client paths and external callback quiescence remain
**UNPROVEN**; PJSUA2 integration readiness remains **BLOCKED**.

## Verification scope

Retained evidence covers:

- historical preview-mode manual GUI acceptance;
- the controlled live PJSUA2 lifecycle observation;
- repeated 100,000-event MPMC stress;
- Clang 18 ThreadSanitizer verification.

Normal unit/integration/source-contract results are reproducible from the current tests and verification scripts and do not need separate per-commit evidence files.

P8-R2A is an **opt-in live experiment** with a bounded 2026-10-12
maintainer-reported observation retained in `p8-r2a-live-stop-reuse-20261012.txt`.
The record includes one rejected-call FAIL and two subsequent stop/reuse PASS
runs. The PASS runs support only the production outgoing-client lifecycle
after a pending call attempt, **not** successful target delivery, real
`PJ_EBUSY` recovery, or universal callback quiescence.

P8-R2B adds a guarded opt-in lab for observing early, real `PJ_EBUSY` on the
production-owned outgoing Account. The maintainer-supplied 2026-10-12 live
result (`p8-r2b-live-pj-ebusy-20261012.txt`) is recorded as one **PASS**
within that bounded lab sequence, subject to comparison with the operator-local
original summary before commit. Do not infer external callback quiescence,
normal-stop `PJ_EBUSY` behavior, or other clients' recovery from this result.

P8-R3A introduces reproducible, offline-only source-contract and concurrent
local ingress regression tests. The canonical CTest entry
`Phase8R3CallbackTargetContractTest` fails when a production callback
method is added or its gate/owner lifetime ordering is changed without
review. `PjsipCallbackActivityTest` now includes an after-close rejected
entry running on a separate thread. These are **not new live evidence**, do
not attest to callbacks that have not yet entered the C++ override, and do
not authorize a change to `UNPROVEN` or `BLOCKED`.

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

P8-R2A was observed live on 2026-10-12: one rejected-call FAIL followed by
two PASS stop/reuse runs with an outgoing call response still `PENDING`. Each
PASS run recorded a disconnect, two returned stops, same-object restart, a
second successful registration, returned destructor, and process exit `0`.
The target extension was not confirmed to have received or answered the call.

`pjsip-live-lifecycle.txt` records a separate controlled PJSUA2 lifecycle run. It observed `PJ_EBUSY`, successful retry after hangup/call-slot cleanup, and no late registration callback in the recorded observation window.

The P8-R2B outgoing production-owned Account lab observed early non-forced
`PJ_EBUSY`, retained owner pointers/validity, disconnect, stop/restart, a second
registration, final stop/destructor and process exit `0`. The subsequent
production `stop()` did not separately record its own vendor shutdown code.
None of these observations proves universal external callback quiescence.

## Artifacts

- `manual-verification.txt` — historical GUI usability/window-close observation.
- `pjsip-live-lifecycle.txt` — controlled live PJSUA2 lifecycle observation.
- `p8-r2a-live-stop-reuse-20261012.txt` — bounded production-client outgoing stop/reuse observations (one FAIL, two PASS).
- `p8-r2b-live-pj-ebusy-20261012.txt` — one real `PJ_EBUSY` and bounded outgoing-client recovery observation (one PASS).
- `stress-test.txt` — 100,000-event MPMC stress summary.
- `tsan-report.txt` — ThreadSanitizer summary and scope.

Per-commit closure/hotfix logs are intentionally left to Git history rather than retained in the current evidence tree.

## Results

- canonical automated verification: PASS within recorded scope;
- MPMC stress: PASS;
- Clang 18 TSan: PASS;
- historical preview-mode GUI acceptance: PASS;
- controlled live PJSUA2 `PJ_EBUSY` recovery: OBSERVED/EVIDENCED;
- P8-R2A production outgoing stop/reuse after pending call attempt: **OBSERVED/EVIDENCED** (two PASS runs; one earlier rejected-call FAIL);
- P8-R2B real outgoing production-owned Account `PJ_EBUSY` / early-shutdown recovery: **OBSERVED/EVIDENCED** (one bounded run);
- other production-client recovery paths: **UNPROVEN**;
- live external callback quiescence: **UNPROVEN**;
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

P8-R2B runs only after the operator approves active-call shutdown with
`1002` calling `1001`. Run it once, then inspect the allowlisted local
summary, preserving all FAIL and INCONCLUSIVE outcomes rather than retrying
to obtain a PASS:

```bash
PHASE8_LIVE_SIP=1 PHASE8_R2B_CONFIRM=1002-to-1001-active-shutdown \
    bash scripts/phase8/run-production-busy-recovery-lab.sh
```

For P8-R2A, after explicitly approving a lab call to extension `1001`, run:

```bash
PHASE8_LIVE_SIP=1 PHASE8_R2A_CONFIRM=1002-to-1001 \
    bash scripts/phase8/run-production-stop-reuse-lab.sh
```

It outputs a sanitized, allowlisted local summary (default: `~/Downloads/`).
The maintainer must review the summary and the source tree state before any
new evidence is committed; raw vendor logs and credentials are discarded.

A new run is a new observation; do not rewrite historical results to appear current.

## References

- `docs/phase8/README.md`
- `docs/architecture/threading-model.md`
- `docs/interview/phase8-questions.md`
- `scripts/phase8/`
- `tests/unit/`
- `tests/integration/`
