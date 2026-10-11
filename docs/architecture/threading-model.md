# Phase 8 Architecture — Threading Model

## Status

Phase 8 makes the concurrency model explicit and testable, but the current
architecture is intentionally split between the production Qt/PJSUA2 path and
the generic C++20 queue/worker path introduced for concurrency verification.

The generic `ThreadSafeQueue<T>` and `SerialTaskWorker` are implemented and
verified in isolation. They are **not** the deployed transport between PJSUA2
callbacks and Qt UI state. Production PJSUA2 shutdown integration remains
fail-closed where real external callback quiescence and `PJ_EBUSY` recovery are
not proven.

Recorded final offline acceptance is PASS for its stated scope. Final Phase 8
maintainer sign-off is still pending.

## Purpose

This document defines:

- the owner/thread boundaries used by the released Qt softphone;
- the contracts of the generic Phase 8 queue and serial worker;
- shutdown and cancellation semantics;
- what Phase 8 stress/TSan tests cover;
- what the PJSUA2 lifetime audits establish; and
- which safety properties remain unproven.

It prevents roadmap diagrams, isolated concurrency tests, and source audits
from being mistaken for stronger runtime guarantees than the evidence supports.

## Scope

Covered:

- `src/concurrency/ThreadSafeQueue.hpp`;
- `src/concurrency/SerialTaskWorker.hpp`;
- `QtRegistrationBackend` session worker and queued GUI handoff;
- `GuiViewModel` / `CallManager` owner-thread contract;
- Phase 8 ownership, MPMC stress, callback activity, shutdown policy, and TSan
  tests;
- bounded PJSUA2 call-slot cleanup and shutdown authorization logic;
- final offline GUI/runtime acceptance.

Not established by this architecture document:

- that PJSUA2 endpoint/account/call objects are universally thread-safe;
- that a zero call-slot count means all external callbacks are quiescent;
- that the generic queue is used by the production Qt event path;
- that dedicated runtime Metrics and Logging threads from the roadmap sketch
  were implemented;
- safe real-world teardown after all PJSUA2 shutdown failures.

## Architecture model

### Production Qt/PJSUA2 event path

The deployed GUI path retains the Phase 7 ownership model:

```text
Qt GUI / application owner thread
    GuiViewModel
        -> CallManager

QtRegistrationBackend
    owns registration/session std::jthread
        -> RegistrationSession
        -> PJSUA2 client/account/call operations

worker event
    -> QMetaObject::invokeMethod(..., Qt::QueuedConnection)
    -> Qt object thread
    -> GuiViewModel / CallManager
```

`CallManager` records its constructing thread and asserts its owner-thread
contract. `QtRegistrationBackend` serializes its session workflow on its owned
`std::jthread` and publishes GUI-facing events through queued Qt delivery.

This path existed before the generic Phase 8 queue/worker.

### Generic Phase 8 concurrency path

```text
multiple producers
      |
      v
ThreadSafeQueue<T>
      |
      v
one or more test consumers

or

multiple task producers
      |
      v
SerialTaskWorker
      |
      v
one owned worker thread
```

`ThreadSafeQueue<T>` is an unbounded C++20 MPMC FIFO with `push`, `try_pop`,
blocking `wait_pop`, stop-token-aware `wait_pop`, and idempotent `close`.

`SerialTaskWorker` owns one `std::jthread` and one
`ThreadSafeQueue<std::function<void()>>`. Accepted tasks execute serially on
the owned worker thread. Throwing tasks are contained and counted rather than
terminating the worker.

### Roadmap diagram boundary

`README_roadmap.md` illustrates SIP, Metrics, and Logging worker threads. That
is an explanatory target model, not evidence that separate metrics/logging
workers are currently deployed. The code-level diagram source is
`docs/architecture/threading-diagram.dot`; its rendered PNG is retained at
`evidence/phase8/threading-diagram.png`.

## Runtime and lifecycle

### `ThreadSafeQueue<T>` lifecycle

- `push()` linearizes under the queue mutex and rejects new work after close.
- `try_pop()` removes at most one available item.
- blocking `wait_pop()` wakes for queued data or queue closure.
- stop-aware `wait_pop(std::stop_token)` additionally wakes for local
  cancellation; cancelling one consumer does not close the shared queue.
- a stop observed before dequeue returns cancellation without consuming an
  item; a concurrent stop/dequeue race may resolve either way.
- `close()` is idempotent and wakes all waiters; accepted data remains
  drainable.
- `size()` and `is_closed()` are point-in-time observations only.
- the queue does not own producer/consumer threads; owners must join users
  before destroying the queue.

The queue does not call Qt or PJSIP APIs.

### `SerialTaskWorker` lifecycle

- construction starts one owned worker thread;
- `post()` accepts non-empty callable tasks until close/shutdown;
- callbacks execute one at a time in queue order;
- `request_close()` may be requested by a callback;
- external `shutdown()` closes, drains, and joins;
- concurrent external shutdown calls serialize their join;
- a callback may not self-join through `shutdown()`;
- owner destruction must not race with active API users or callbacks capturing
  owner resources;
- callback exceptions increment `failures()` while `completed()` counts all
  finished callbacks.

The worker thread is not automatically the Qt GUI thread or a valid PJSUA2
owner thread.

### PJSUA2 call cleanup and shutdown boundary

Phase 8 hardened the outgoing/incoming client cleanup ordering so call-slot
cleanup is observed before destroying the derived Call callback target where
possible. A deterministic polling policy makes zero-slot, delayed cleanup,
timeout, and deadline behavior testable.

Later commits model:

- callback activity/drain behavior;
- shutdown failure paths;
- external callback ingress races;
- ownership retries and stale results;
- production-facing shutdown authorization; and
- a fail-closed integration contract.

These models deliberately distinguish **call-slot state** from **external
callback quiescence**. `pjsua_call_get_count() == 0` does not prove all vendor
callbacks have stopped. If non-forced shutdown fails and safe callback
quiescence cannot be established, integration readiness remains BLOCKED.

## Verification strategy

### Deterministic/unit coverage

Standalone `cmake/phase8-focused` harness covers queue behavior, serial worker behavior,
worker-thread ownership, MPMC stress, PJSIP call-slot cleanup policy, callback
activity, and shutdown/lifetime models. The Phase 8 subtree is also registered
with canonical CTest.

### Stress coverage

The primary integrity stress uses four producers × 25,000 events and four
consumers for 100,000 distinct IDs. Atomic per-ID counters detect missing,
duplicate, malformed, or unexpectedly rejected items without adding a test-side
data race. Separate tests cover per-producer FIFO and close/drain accounting.

Stress proves the exercised invariants under the observed schedules; it does
not provide deterministic coverage of all possible thread interleavings.

### ThreadSanitizer scope

TSan is opt-in for the standalone Phase 8 C++ tests. The verified ThinkPad
Clang 18 run executes six standalone tests three times each, yielding 18/18
PASS executions. GCC 13 on the ThinkPad was BLOCKED by runtime memory-mapping
failure and was not mislabeled as a clean sanitizer run.

TSan does not instrument the Qt GUI executable or vendor PJSUA2 runtime in the
recorded Phase 8 evidence. Therefore it cannot establish race freedom for those
unexercised components.

### Production-path ownership/lifetime verification

Source audits and focused Qt fake-session regressions verify the intended
ownership/queued-handoff contracts without making live SIP claims. PJSUA2
shutdown integration audits are fail-closed: the verifier can PASS its audit
while still reporting integration readiness BLOCKED.

### Final offline runtime acceptance

The recorded ThinkPad acceptance includes canonical 147/147 CTest, Qt worker
repeats, focused Phase 8 tests, repeated MPMC stress, Clang 18 TSan, non-live
PJSIP regression, roadmap audit, 40/40 headless GUI startup/exit runs, and
human preview-mode GUI interaction/normal X-close.

It deliberately does not perform live SIP registration/calls while PJSUA2
shutdown lifetime risks remain unresolved.

## Design rationale

### Prefer explicit ownership over "thread-safe everywhere"

The design keeps ownership boundaries narrow: application state belongs to its
owner thread; GUI-facing delivery uses Qt queued invocation; generic workers do
not gain implicit permission to access Qt or PJSUA2 objects.

### Keep cancellation local unless shutdown is global

A stop request to one queue consumer should not close a shared queue for other
users. Queue `close()` represents global no-more-work/drain semantics, while a
stop token represents local waiter cancellation.

### Drain accepted work deterministically

`SerialTaskWorker` uses close-and-drain shutdown so accepted tasks are
accounted for. The project does not currently expose an abort-pending-work
mode or bounded backpressure; these limitations are explicit rather than
hidden.

### Separate evidence scopes

Unit tests, MPMC stress, TSan, source audits, Qt fake-session tests, and offline
GUI acceptance prove different properties. Phase 8 keeps those scopes
separate so one passing gate is not used as evidence for an unrelated runtime
claim.

### Fail closed on unresolved PJSUA2 lifetime questions

A shutdown authorization helper and integration audit exist specifically to
avoid treating incomplete lifetime evidence as permission to destroy PJSUA2
objects. BLOCKED/UNPROVEN is an acceptable technical result when the underlying
library boundary cannot yet be demonstrated safe.

## Phase boundary

Phase 8 establishes a reusable concurrency foundation and a stronger,
evidence-backed ownership/lifetime model. It proves the exercised queue,
worker, stress, sanitizer, non-live shutdown-contract, and offline GUI
acceptance properties recorded in the repository.

Phase 8 does **not** prove:

- that the generic queue transports current Qt/PJSUA2 production events;
- live PJSUA2 callback quiescence;
- safe destruction after every shutdown failure;
- actual `PJ_EBUSY` recovery in the Asterisk lab;
- universal race freedom;
- memory leak freedom; or
- sustained GUI rendering performance.

Those unresolved production PJSUA2 lifetime risks must remain visible at phase
closure and be carried forward unless new live evidence resolves them.

## References

- `README_roadmap.md`, Phase 8 — Multithreading.
- `docs/phase8/README.md`.
- `docs/phase8/roadmap-exit-review.md`.
- `docs/phase8/final-runtime-acceptance.md`.
- `docs/architecture/threading-diagram.dot`.
- `docs/interview/phase8-questions.md`.
- `evidence/phase8/README.md`.
- `evidence/phase8/roadmap-closure.md`.
- `tests/unit/` and `tests/integration/`, with focused registration under `cmake/phase8-focused/`.
- `scripts/phase8/`.
