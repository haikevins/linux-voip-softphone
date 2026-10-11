# Phase 8 — Multithreading

## Status

Phase 8 is merged to `main` and its post-merge automated verification passes. Final maintainer acceptance is still **PENDING**.

Current safety boundary:

- controlled PJSUA2 `PJ_EBUSY` recovery: observed/evidenced in the recorded live probe;
- late registration callbacks after `shutdown2`: none observed in that recorded run;
- live external callback quiescence: **UNPROVEN**;
- production-client real `PJ_EBUSY` recovery: **UNPROVEN**;
- PJSUA2 integration readiness: **BLOCKED**.

There is no separate Phase 8 release tag in the current roadmap.

## Objective

Make the softphone concurrency model explicit and testable through:

- a reusable C++20 thread-safe queue;
- deterministic serial worker ownership and shutdown;
- high-contention producer/consumer stress;
- independent ThreadSanitizer verification;
- explicit Qt/PJSUA2 ownership and lifetime review;
- fail-closed modeling where safe PJSUA2 shutdown cannot be proven.

The generic queue/worker path is verified independently and is not claimed to be the deployed transport for every production Qt/PJSUA2 callback.

## Scope

In scope:

- `ThreadSafeQueue<T>` close, cancellation, drain, ordering, and ownership semantics;
- `SerialTaskWorker` FIFO execution, exception containment, and lifecycle;
- canonical Phase 8 CTest registration;
- repeated 100,000-event MPMC stress;
- standalone Clang 18 ThreadSanitizer verification;
- Qt/PJSUA2 ownership and callback-lifetime audits;
- call-slot cleanup and shutdown-order verification;
- fail-closed shutdown authorization/integration models;
- offline GUI/runtime acceptance and roadmap evidence.

Out of scope:

- new SIP/user-facing call features;
- replacing Qt queued connections with the generic queue;
- claiming universal race freedom or external callback quiescence;
- treating a controlled live recovery observation as proof of production shutdown safety;
- changing or recreating the Phase 7 tag `v0.8-qt-gui`.

## Deliverables

- `ThreadSafeQueue<T>` concurrency primitive.
- Owned `SerialTaskWorker`.
- Unit/integration coverage for queue/worker behavior.
- Repeated 100,000-event MPMC stress verification.
- Standalone Clang 18 TSan verification.
- Qt/PJSUA2 source ownership and lifetime audits.
- Consolidated Phase 8 verification scripts under `scripts/phase8/`.
- Architecture and shutdown-safety documentation.
- Controlled live PJSUA2 lifecycle evidence.
- Final offline runtime/manual GUI acceptance procedure and evidence.
- Roadmap exit review and evidence inventory.

## Architecture and design

The authoritative architecture document is `docs/architecture/threading-model.md`.

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

The shutdown model distinguishes local call-slot cleanup, callback admission/drain, retained callback targets, and external library callback quiescence. A zero active-call count or drained local gate is not proof that vendor callbacks can never re-enter.

## Build and run

Aggregate offline verification:

```bash
bash scripts/phase8/verify.sh
```

Focused concurrency verification:

```bash
bash scripts/phase8/verify-threading.sh queue
PHASE8_STRESS_REPEATS=100 bash scripts/phase8/verify-threading.sh stress
CXX="$(command -v clang++-18)" PHASE8_TSAN_BUILD_DIR="$PWD/build/phase8-tsan-clang18-debug" PHASE8_TSAN_REPEATS=3     bash scripts/phase8/verify-threading.sh tsan
```

PJSIP/Qt offline verification:

```bash
bash scripts/phase8/verify-pjsip.sh shutdown-order
bash scripts/phase8/verify-pjsip.sh production-lifetime-audit
bash scripts/phase8/verify-pjsip.sh shutdown-integration-contract
bash scripts/phase8/verify-pjsip.sh live-evidence
```

Final offline runtime acceptance:

```bash
bash scripts/phase8/verify-final-runtime-acceptance.sh --automatic
```

The controlled live lifecycle probe is explicit opt-in only:

```bash
PHASE8_LIVE_SIP=1 bash scripts/phase8/run-pjsip-live-lifecycle-probe.sh
```

See `docs/phase8/final-runtime-acceptance.md` before running the human GUI stage.

## Verification

Recorded Phase 8 acceptance evidence includes:

- canonical CTest: **147/147 PASS**;
- Qt worker/event-handoff repeat: **10/10 PASS**;
- focused Phase 8 CTest: **12/12 PASS**;
- repeated 100,000-event MPMC stress: **PASS**;
- Clang 18 standalone TSan: **18/18 PASS**;
- PJSIP non-live regression: **45/45 PASS**;
- GUI headless startup/normal exit: **40/40 PASS**;
- historical manual preview-mode GUI acceptance: **PASS**;
- post-merge automated revalidation: **PASS**.

The controlled live PJSUA2 probe demonstrated one `PJ_EBUSY` recovery sequence and observed no late registration callbacks after `shutdown2` in that recorded run. It does not prove external callback quiescence or production-client recovery.

## Evidence

Phase 8 evidence is indexed at `evidence/phase8/README.md`.

Important retained records include roadmap stress/TSan artifacts, threading-diagram evidence, controlled PJSUA2 lifecycle evidence, and final runtime acceptance records. Historical evidence remains provenance-sensitive and is not rewritten merely because documentation or scripts are consolidated.

Do not commit SIP passwords, Digest Authorization responses/nonces/cnonce material, SRTP inline keys, raw sensitive SIP traces, or packet captures containing protected media.

## Interview focus

Be able to explain:

- why the generic queue does not replace Qt queued delivery automatically;
- the difference between queue shutdown, worker shutdown, and PJSUA2 lifetime;
- what the 100,000-event MPMC stress test proves and does not prove;
- why standalone TSan coverage is useful but limited;
- how Qt owner-thread handoff is enforced;
- why call-slot count is not callback quiescence;
- why unresolved PJSUA2 teardown questions remain `BLOCKED/UNPROVEN`.

See `docs/interview/phase8-questions.md`.

## Exit criteria

| Criterion | State |
|---|---|
| `ThreadSafeQueue`, mutex, condition variable, atomic | Implemented/tested |
| Owned serial worker | Implemented/tested in isolation |
| 100,000-event MPMC stress | PASS evidence |
| Standalone TSan | Clang 18 PASS evidence |
| Qt/PJSUA2 ownership/lifetime review | Implemented/audited |
| Offline runtime verification | PASS evidence |
| Live external callback quiescence | UNPROVEN |
| Production-client real `PJ_EBUSY` recovery | UNPROVEN |
| PJSUA2 integration readiness | BLOCKED |
| Final maintainer acceptance | PENDING |
| Merge to `main` | DONE |
| Phase 8 release tag | No separate tag defined |

Final maintainer acceptance must explicitly acknowledge the unresolved PJSUA2 boundary. Merge history is not itself an acceptance signal.

## References

- `README_roadmap.md`
- `docs/architecture/threading-model.md`
- `docs/phase8/pjsip-shutdown-safety.md`
- `docs/phase8/pjsip-live-lifecycle.md`
- `docs/phase8/final-runtime-acceptance.md`
- `docs/phase8/roadmap-exit-review.md`
- `evidence/phase8/README.md`
- `scripts/phase8/`

## Next phase

Do not start Phase 9 until Phase 8 receives explicit maintainer acceptance. The current roadmap assigns `v0.9-diagnostics` to Phase 9 and defines no separate Phase 8 release tag.
