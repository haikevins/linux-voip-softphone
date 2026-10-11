# Phase 8 Architecture — Threading Model

## Status

**PENDING** — Phase 8 is merged to `main`, and its concurrency foundation and offline ownership/shutdown contracts are implemented and tested. Final maintainer acceptance has not been recorded.

Controlled live PJSUA2 `PJ_EBUSY` recovery was observed, but live external callback quiescence and production-client real `PJ_EBUSY` recovery remain **UNPROVEN**; PJSUA2 integration readiness remains **BLOCKED**.

## Purpose

Define the current ownership/thread boundaries, queue/worker lifecycle rules, verification scope, and unresolved PJSUA2 shutdown boundary.

## Scope

Covered:

- `src/concurrency/ThreadSafeQueue.hpp`;
- `src/concurrency/SerialTaskWorker.hpp`;
- `QtRegistrationBackend` worker/session ownership and queued UI delivery;
- `GuiViewModel` / `CallManager` owner-thread contract;
- PJSUA2 callback activity, call-slot cleanup, shutdown authorization, and source-order contracts;
- stress and ThreadSanitizer coverage.

Not claimed:

- that the generic queue is the production Qt event transport;
- universal PJSUA2 object thread safety;
- universal external callback quiescence;
- safe production teardown after every possible shutdown failure.

## Architecture model

### Production Qt/PJSUA2 path

```text
Qt GUI / application owner thread
    GuiViewModel -> CallManager

QtRegistrationBackend
    owns session std::jthread
        -> RegistrationSession / PJSUA2 client
        -> QMetaObject::invokeMethod(..., Qt::QueuedConnection)
        -> QObject/UI owner thread
```

`CallManager` enforces its owner-thread contract. `QtRegistrationBackend` serializes session work on its owned worker and uses queued Qt delivery before GUI-facing state is mutated.

### Generic concurrency path

```text
multiple producers -> ThreadSafeQueue<T> -> consumers

multiple producers -> SerialTaskWorker -> one owned worker thread
```

`ThreadSafeQueue<T>` is an unbounded C++20 MPMC FIFO with nonblocking/blocking pop, stop-token-aware waiting, and idempotent close.

`SerialTaskWorker` owns one `std::jthread`, accepts tasks until close, executes accepted tasks serially, contains task exceptions, and drains accepted work before external shutdown returns.

The generic worker is not automatically a Qt or PJSUA2 owner thread.

### PJSUA2 boundary

Phase 8 hardens explicit stop ordering, call-slot cleanup, callback admission/drain, callback-target lifetime, and fail-closed shutdown authorization.

`pjsua_call_get_count() == 0` is only call-slot state. It does not prove that every external vendor callback source is quiescent. If that stronger property is unavailable, object-release authorization remains blocked.

## Runtime and lifecycle

### Queue

- successful pushes linearize under the queue mutex;
- `close()` rejects future pushes and wakes waiters;
- accepted data remains drainable after close;
- stop-token cancellation is local to the waiting consumer and does not close the shared queue;
- queue destruction requires owners to stop/join concurrent users first.

### Serial worker

- construction starts one owned worker;
- accepted tasks execute in queue order;
- `request_close()` stops admission without self-joining;
- external `shutdown()` closes, drains, and joins;
- task exceptions are counted rather than terminating the worker;
- owner destruction must not race with callers or callbacks using owner resources.

### PJSUA2 shutdown

Production clients retain callback targets through the reviewed `libDestroy()` ordering and fail closed when explicit stop cannot establish the modeled release conditions.

`src/pjsip/PjsipAccountShutdownAttempt.hpp` is the shared exception-to-result
boundary now used by registration, outgoing, and incoming production clients.
Each real client still supplies `shutdown2(force=false)`, preserves its
client-specific logging, and keeps the stop-level failure veto over Account and
Endpoint release. A deterministic fake-operation test covers dispatch,
synthetic busy/exception results, explicit retry, and retained *modeled* owners;
it does not instantiate PJPROJECT or establish production retry success.

A controlled vendor-level live probe observed:

- first `shutdown2(force=false)` -> `PJ_EBUSY` with one active call slot;
- hangup and call-slot cleanup;
- second shutdown -> success;
- no late registration callback in the recorded observation window.

That observation does not prove production-client recovery or universal callback quiescence.

## Verification strategy

All normal C++/Python contract tests are registered through the repository's canonical `tests/CMakeLists.txt`; there is no phase-specific CMake project.

Concurrency stress uses four producers and four consumers with 100,000 distinct IDs plus FIFO and close/drain scenarios. Passing runs establish the exercised invariants, not every possible scheduler interleaving.

ThreadSanitizer uses the root CMake project with `ENABLE_TSAN=ON` in a separate build. The recorded Clang 18 gate passed six selected C++ tests three times each (18/18). Vendor PJSUA2 and the complete Qt runtime were not thereby proven race-free.

Source-contract tests inspect production PJSUA2 shutdown structure and intentionally remain fail-closed: an offline audit may PASS while integration readiness remains BLOCKED.
The shutdown-attempt injection test additionally exercises the helper invoked by all three clients, without attributing fake errors to a live SIP session.

## Design rationale

### Explicit ownership

Thread confinement and explicit queued handoff are preferred to a vague "thread-safe everywhere" assumption.

### Local cancellation versus global close

A consumer stop request should not close a queue shared with other consumers. `close()` represents global no-more-work/drain semantics.

### Drain accepted work

The serial worker accounts for work that was already accepted instead of silently discarding it during shutdown.

### Fail closed at the library boundary

Local callback gates and call-slot cleanup are useful but are not substitutes for proof about external PJSUA2 callback sources. Missing evidence therefore blocks release authorization rather than being converted into a PASS claim.

## Phase boundary

Phase 8 establishes:

- reusable/tested C++20 concurrency primitives;
- repeatable MPMC stress and TSan evidence;
- explicit Qt owner-thread handoff;
- stronger PJSUA2 shutdown/source contracts;
- one controlled live `PJ_EBUSY` recovery observation.

It does **not** establish live external callback quiescence, production-client real `PJ_EBUSY` recovery, universal race freedom, or production PJSUA2 integration readiness.

## References

- `docs/phase8/README.md`
- `docs/interview/phase8-questions.md`
- `evidence/phase8/README.md`
- `tests/unit/`
- `tests/integration/`
- `scripts/phase8/`
