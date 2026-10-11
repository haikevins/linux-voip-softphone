# Phase 8 — Multithreading

Phase 8 is a **closure candidate, not yet merged or released**. The branch is
`phase/8-multithreading`. The latest production-affecting hotfix is
`b6c84ac48f9dbb9d442a446dc05bdd47ec1f08ca`; documentation/evidence Commit 34
is `ba050bb9bcdd01391e2860b16e2f273c824b3ddc`.

GitHub CI completed **3/3 SUCCESS** on the hotfix commit: Build, Static Analysis,
and Tests and Sanitizers all passed. The final offline runtime acceptance on
that production commit also passed, including the manual GUI gate. The current
runtime record is `evidence/phase8/final-runtime-acceptance-b6c84ac.txt`.

The Phase 8 documentation and verification entry points are intentionally
consolidated. Historical per-commit documents and one-off wrapper scripts remain
available in Git history and in the chronological evidence that recorded their
original paths. Consolidation does not rewrite that evidence and does not turn
historical observations into new current-head runtime proof.

## Status

### Current safety boundary

The strongest current claims are deliberately narrow:

- controlled PJSUA2 2.17 `PJ_EBUSY` recovery behavior: **LIVE OBSERVED /
  EVIDENCED** in the recorded Commit 30 lab run;
- late registration callback after successful account shutdown in that recorded
  run: **NONE OBSERVED**;
- Commit 31 production registration/teardown observation: **3/3 PASS** on the
  exact source bytes later committed as `6b19d5e`;
- live external callback quiescence as a general guarantee: **UNPROVEN**;
- production-client recovery from an injected real `PJ_EBUSY`: **UNPROVEN**;
- PJSUA2 production integration readiness: **BLOCKED**.

Do not describe Phase 8 as proving universal race freedom, universal PJSUA2
callback quiescence, live SIP/media teardown safety, leak freedom, or sustained
GUI frame-rate performance.

## Objective

Make the softphone concurrency model explicit and testable through:

- a reusable C++20 thread-safe queue;
- deterministic serial worker ownership and shutdown behavior;
- high-contention producer/consumer stress testing;
- independent ThreadSanitizer verification;
- explicit Qt/PJSUA2 ownership and lifetime review;
- fail-closed modeling of unsafe PJSUA2 shutdown cases; and
- repeatable offline GUI/runtime acceptance without changing Phase 7 SIP
  behavior.

The generic `ThreadSafeQueue<T>` / `SerialTaskWorker` path remains separate
from the production Qt/PJSUA2 event path. Phase 8 does not claim that the
generic queue replaced Qt queued delivery.

## Scope

In scope:

- `ThreadSafeQueue<T>` queue semantics, cancellation, close/drain behavior,
  producer/consumer integrity, and ownership requirements;
- `SerialTaskWorker` FIFO execution, exception containment, lifecycle, and
  non-reentrant task handoff;
- canonical CTest registration of Phase 8 tests;
- 100,000-event MPMC stress and repeated stress execution;
- opt-in standalone TSan with documented toolchain boundaries;
- Qt/PJSUA2 source ownership and callback-lifetime audits;
- bounded call-slot cleanup and deterministic cleanup policy tests;
- callback activity, shutdown failure-path, stale-result, and external-ingress
  race models;
- shutdown-authorization policy and integration contract checks;
- roadmap-required threading evidence and final offline GUI acceptance.

Out of scope:

- new user-facing call features or SIP account features;
- replacing Qt queued connections with the generic Phase 8 queue;
- claiming dedicated Metrics/Logging runtime threads exist merely because the
  roadmap diagram illustrates them;
- live SIP registration/call verification as part of the offline final runtime
  gate;
- treating one controlled live PJSUA2 recovery observation as proof of
  universal callback quiescence or production-client shutdown safety;
- changing or recreating the Phase 7 tag `v0.8-qt-gui`.

## Deliverables

### Consolidated documentation

Current Phase 8 documentation is intentionally limited to five files:

- `README.md` — phase scope, current status, entry points and exit criteria;
- `pjsip-shutdown-safety.md` — consolidated ownership/lifetime/shutdown design
  and verification boundary;
- `pjsip-live-lifecycle.md` — controlled live PJSUA2 lifecycle observation;
- `final-runtime-acceptance.md` — current offline runtime/manual GUI protocol and
  recorded results;
- `roadmap-exit-review.md` — roadmap requirements, evidence mapping and
  maintainer sign-off boundary.

The former `commit7-*` through `commit19-*`, `commit30-*`, `commit32-*`, and
`commit34-*` documents are historical provenance. Their content is represented
by the consolidated documents above and remains recoverable from Git history.

## Architecture and design

The authoritative architecture document is
[`docs/architecture/threading-model.md`](../architecture/threading-model.md).
The key boundary is:

```text
Qt/PJSUA2 production path
    QtRegistrationBackend std::jthread
        -> RegistrationSession / PJSUA2 client
        -> QMetaObject::invokeMethod(..., Qt::QueuedConnection)
        -> GuiViewModel / CallManager owner thread

Generic Phase 8 path
    ThreadSafeQueue<T>
        -> SerialTaskWorker
        -> standalone concurrency/stress/TSan tests
```

For PJSUA2 lifetime work, call-slot cleanup, callback admission/drain, retained
callback targets, fail-closed destruction and shutdown-authorization models are
defensive contracts. A zero active-call count or a drained local callback gate
is not by itself proof that the external library can never enter another
callback.

## Build and run

### Verification entry points

Threading/focused verification:

```bash
bash scripts/phase8/verify-threading.sh queue
PHASE8_STRESS_REPEATS=100 bash scripts/phase8/verify-threading.sh stress
CXX="$(command -v clang++-18)" \
PHASE8_TSAN_BUILD_DIR="$PWD/build/phase8-tsan-clang18-debug" \
PHASE8_TSAN_REPEATS=3 \
    bash scripts/phase8/verify-threading.sh tsan
```

Local reproduction of GitHub static-analysis gates:

```bash
bash scripts/phase8/verify-static-analysis.sh all
```

PJSIP/Qt offline verification is consolidated behind subcommands:

```bash
bash scripts/phase8/verify-pjsip.sh shutdown-order
bash scripts/phase8/verify-pjsip.sh production-lifetime-audit
bash scripts/phase8/verify-pjsip.sh shutdown-integration-contract
bash scripts/phase8/verify-pjsip.sh live-evidence
```

The controlled live experiment remains a separate explicit opt-in action and is
never run by the offline aggregate:

```bash
PHASE8_LIVE_SIP=1 bash scripts/phase8/run-pjsip-live-lifecycle-probe.sh
```

Final offline automatic acceptance:

```bash
bash scripts/phase8/verify-final-runtime-acceptance.sh --automatic
```

The human GUI stage must be run from a graphical desktop using the evidence
directory emitted by the automatic stage. See `final-runtime-acceptance.md`.

The aggregate offline gate is:

```bash
bash scripts/phase8/verify.sh
```

## Verification

### Recorded current production-head acceptance

The final acceptance rerun tied to hotfix `b6c84ac` recorded:

- GitHub CI: **3/3 SUCCESS**;
- canonical CTest: **147/147 PASS**;
- Qt worker/event-handoff repeat: **10/10 PASS**;
- focused Phase 8 CTest: **12/12 PASS**;
- repeated 100,000-event MPMC stress: **PASS**;
- Clang 18 standalone TSan: **18/18 PASS**;
- PJSIP non-live regression: **45/45 PASS**;
- GUI headless startup/normal exit: **40/40 PASS**;
- manual preview-mode GUI acceptance: **PASS**, 92.97 seconds, 185 samples,
  normal exit `0`.

These observations remain tied to their recorded commit/evidence. A later
document/script consolidation must be reverified before maintainer sign-off;
consolidation alone is not new runtime evidence.

## Evidence

Chronological Phase 8 evidence is retained under `evidence/phase8/`. Roadmap
closure artifacts remain under `evidence/07-threading/`. Historical evidence is
not rewritten merely because current documentation and scripts were
consolidated.

The controlled live lifecycle summary is
`evidence/phase8/pjsip-live-lifecycle.txt`. Raw SIP passwords, Digest
Authorization material, nonce/cnonce/response material, SRTP inline keys and raw
packet captures must not be committed.

## Interview focus

The review-oriented explanation for this phase is
[`docs/interview/phase8-questions.md`](../interview/phase8-questions.md).
Important topics include:

- why the generic queue is not automatically a replacement for Qt queued
  connections;
- the difference between queue shutdown, worker shutdown, and PJSUA2 object
  lifetime;
- what the 100,000-event MPMC stress test proves and does not prove;
- why isolated TSan coverage is useful but limited;
- how owner-thread rules are enforced for `CallManager` and Qt handoff;
- why call-slot count is not callback quiescence; and
- why Phase 8 records `BLOCKED/UNPROVEN` instead of manufacturing a PASS for
  live PJSUA2 teardown.

## Exit criteria

| Criterion | State |
|---|---|
| `ThreadSafeQueue`, mutex, condition variable, atomic | Implemented/tested |
| Owned serial worker thread | Implemented/tested in isolation |
| 100,000 events with multiple producers/consumers | PASS evidence |
| ThreadSanitizer where feasible | Clang 18 standalone PASS evidence |
| Qt/PJSUA2 ownership and lifetime review | Implemented/audited |
| Offline GUI/runtime acceptance | PASS on `b6c84ac`; consolidation revalidation required |
| GitHub CI | 3/3 PASS on `b6c84ac`; consolidation commit must pass again |
| Live external callback quiescence | UNPROVEN |
| Production-client real `PJ_EBUSY` recovery | UNPROVEN |
| PJSUA2 integration readiness | BLOCKED |
| Final maintainer Phase 8 sign-off | PENDING |
| Merge to `main` / release tag | NOT STARTED |

Final maintainer acceptance must explicitly acknowledge the remaining PJSUA2
boundary. No merge, tag, or history rewrite is authorized by this document.

## References

- [`README_roadmap.md`](../../README_roadmap.md), Phase 8 — Multithreading.
- [`docs/architecture/threading-model.md`](../architecture/threading-model.md).
- [`pjsip-shutdown-safety.md`](pjsip-shutdown-safety.md).
- [`pjsip-live-lifecycle.md`](pjsip-live-lifecycle.md).
- [`final-runtime-acceptance.md`](final-runtime-acceptance.md).
- [`roadmap-exit-review.md`](roadmap-exit-review.md).

## Next phase

Do not start Phase 9 from this branch until Phase 8 is explicitly accepted,
merged to `main`, synchronized, and (if the project release plan requires it)
tagged.

The roadmap next phase is Phase 9 — debugging, memory, and core dump. The open
production PJSUA2 shutdown risks from Phase 8 must remain visible as carried
technical debt unless they are resolved with real evidence before Phase 8
closure.



<!-- PHASE8_MANUAL_ACCEPTANCE_BA050BB -->

### Final manual runtime acceptance refresh

A final graphical GUI acceptance run was completed on parent runtime HEAD
`ba050bb9bcdd01391e2860b16e2f273c824b3ddc`.

Result: **PASS**.

The preview/no-SIP UI remained responsive through the required interaction
and idle-soak checks and exited normally through the window close path.

The complete observed record is:

`evidence/phase8/final-runtime-acceptance-ba050bb.txt`

This refresh is offline GUI evidence only. It does not establish live SIP
acceptance, external callback quiescence, or universal PJSUA2 shutdown safety.
Those boundaries remain explicitly **BLOCKED/UNPROVEN** where already
documented.
