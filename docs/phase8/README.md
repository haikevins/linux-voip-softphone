# Phase 8 — Multithreading

## Status

**PENDING** — Phase 8 is merged to `main` and its automated verification passes, but final maintainer acceptance has not been recorded.

The unresolved production boundary is unchanged:

- controlled PJSUA2 `PJ_EBUSY` recovery was observed in the recorded live probe;
- no late registration callback was observed after `shutdown2` in that recorded run;
- live external callback quiescence is **UNPROVEN**;
- production-client real `PJ_EBUSY` recovery is **UNPROVEN**;
- PJSUA2 integration readiness is **BLOCKED**.

There is no separate Phase 8 release tag.

## Objective

Make concurrency ownership and shutdown behavior explicit and testable without changing the Phase 7 user-facing call feature set.

## Scope

Included in this phase:

- `ThreadSafeQueue<T>` close, cancellation, FIFO, and drain semantics;
- owned `SerialTaskWorker` lifecycle and exception containment;
- repeated 100,000-event MPMC stress;
- ThreadSanitizer verification;
- Qt owner-thread handoff;
- PJSUA2 call-slot/callback/shutdown lifetime contracts;
- one controlled live PJSUA2 lifecycle observation.

Deliberately excluded:

- replacing Qt queued delivery with the generic queue;
- claiming universal PJSUA2 callback quiescence or teardown safety;
- treating offline verification or merge to `main` as final Phase 8 acceptance.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| Generic queue | Own MPMC FIFO, close, cancellation, and drain semantics | `src/concurrency/ThreadSafeQueue.hpp` |
| Serial worker | Own one worker thread and accepted-work drain | `src/concurrency/SerialTaskWorker.hpp` |
| Unit/integration tests | Exercise deterministic behavior within canonical CTest registration | `tests/unit/`, `tests/integration/` |
| Account shutdown invocation | Share fail-closed error handling; exercise a fake `PJ_EBUSY`-like error without a SIP server | `src/pjsip/PjsipAccountShutdownAttempt.hpp`, `tests/unit/test_pjsip_account_shutdown_attempt.cpp` |
| Offline verification | Run concurrency, 100,000-event stress, and TSan checks | `scripts/phase8/verify.sh`, `scripts/phase8/verify-threading.sh` |
| Live probe | Record bounded PJSUA2 lifecycle observations through explicit opt-in | `scripts/phase8/run-pjsip-live-lifecycle-probe.sh` |
| Architecture | Document Qt/PJSUA2 ownership and unproven shutdown boundaries | `docs/architecture/threading-model.md` |
| Evidence | Preserve sanitized stress, TSan, GUI, and controlled live observations | `evidence/phase8/README.md` |

## Architecture and design

`docs/architecture/threading-model.md` is the architecture source of truth.

The production GUI retains the Phase 7 model:

```text
QtRegistrationBackend session worker
        -> PJSUA2 session/client operations
        -> Qt::QueuedConnection
        -> QObject/UI owner thread
```

The generic concurrency primitives are independently reusable:

```text
producers -> ThreadSafeQueue<T> -> consumers
producers -> SerialTaskWorker   -> one owned worker thread
```

These paths are related by ownership principles, not by an assertion that the generic queue transports every production Qt/PJSUA2 callback.

## Build and run

Aggregate offline verification:

```bash
bash scripts/phase8/verify.sh
```

Concurrency/stress verification:

```bash
bash scripts/phase8/verify-threading.sh core
THREADING_STRESS_REPEATS=100 bash scripts/phase8/verify-threading.sh stress
```

ThreadSanitizer uses the normal root CMake project in a separate build:

```bash
CXX="$(command -v clang++-18)" \
THREADING_TSAN_REPEATS=3 \
bash scripts/phase8/verify-threading.sh tsan
```

The live lifecycle probe is explicit opt-in only:

```bash
PHASE8_LIVE_SIP=1 bash scripts/phase8/run-pjsip-live-lifecycle-probe.sh
```

## Verification

Recorded Phase 8 results include:

- canonical CTest: **147/147 PASS** on the recorded final gate;
- repeated 100,000-event MPMC stress: **PASS**;
- Clang 18 TSan: **18/18 PASS** in the recorded six-test gate;
- Qt/PJSIP ownership and shutdown source regressions: **PASS** within their offline scope;
- historical preview-mode manual GUI acceptance: **PASS**;
- controlled live PJSUA2 `PJ_EBUSY` recovery: **OBSERVED/EVIDENCED**.

The P8-R1 shutdown-attempt regression injects synthetic shutdown failures through
one helper now invoked by all three production clients. It verifies fail-closed
result translation and a modeled explicit retry; **it does not exercise real
PJSUA2 client recovery or establish external callback quiescence**. This new
test is additive to the previously recorded 147-test baseline.

Offline PASS does not upgrade the unresolved live production shutdown boundary.

## Evidence

The concise evidence index is `evidence/phase8/README.md`.

Retained artifacts cover manual GUI acceptance, controlled live PJSUA2 lifecycle behavior, MPMC stress, and TSan. Commit-by-commit closure logs are intentionally left to Git history rather than duplicated in the current evidence tree.

Never commit SIP passwords, Digest response/nonce/cnonce material, SRTP inline keys, raw sensitive SIP traces, or protected packet captures.

## Interview focus

Be able to explain:

- queue close versus local stop-token cancellation;
- serial-worker ownership and drain semantics;
- what MPMC stress and TSan do and do not prove;
- why Qt queued delivery remains the production UI handoff;
- why call-slot count is not external callback quiescence;
- why the current PJSUA2 integration boundary remains fail-closed.

See `docs/interview/phase8-questions.md`.

## Exit criteria

| Criterion | State |
|---|---|
| Queue/worker implementation and deterministic tests | PASS |
| 100,000-event MPMC stress | PASS evidence |
| Clang 18 TSan | PASS evidence |
| Qt owner-thread handoff regression | PASS |
| Controlled PJSUA2 `PJ_EBUSY` recovery | OBSERVED/EVIDENCED |
| Live external callback quiescence | UNPROVEN |
| Production-client real `PJ_EBUSY` recovery | UNPROVEN |
| PJSUA2 integration readiness | BLOCKED |
| Final maintainer acceptance | PENDING |
| Separate Phase 8 release tag | None defined |

Final acceptance must explicitly acknowledge the unresolved PJSUA2 boundary; merge history alone is not acceptance.

## References

- `ROADMAP.md`
- `docs/architecture/threading-model.md`
- `docs/interview/phase8-questions.md`
- `evidence/phase8/README.md`
- `tests/unit/`
- `tests/integration/`
- `scripts/phase8/`

## Next phase

Do not start Phase 9 until Phase 8 receives explicit maintainer acceptance. The roadmap assigns `v0.9-diagnostics` to Phase 9.
