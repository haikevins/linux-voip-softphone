# Threading model — Phase 8

## Provenance and current baseline

According to the Phase 7 handoff for `v0.8-qt-gui`, the GUI backend already
uses `std::jthread`, mutex/condition-variable synchronization, and Qt queued
handoff to the GUI. `CallManager` has an owner-thread contract. These are
**existing boundaries**; this document does not assert that a fresh runtime
trace or code audit of the canonical clone was completed for Commit 1.

The queue introduced in Phase 8 Commit 1 and extended in Commit 2 is an
**isolated library component**. Commit 3 adds a standalone
`SerialTaskWorker` that owns a thread and consumes callable tasks from the
queue. Neither component is wired to Qt, `CallManager`, or PJSUA2.
Commits 1–3 were tested in the canonical ThinkPad repository on
2026-10-09 (maintainer-supplied terminal logs). Commit 4 adds an explicit
worker thread-ownership test and a guarded canonical CTest registration step.
It does **not** establish Qt/PJSIP callback thread affinity.

## Current vs. planned event paths

```text
Phase 7 baseline (reported in release handoff)
PJSUA2/session worker -> Qt queued handoff -> GUI state/UI thread
                  CallManager: owner-thread affinity

Phase 8 Commits 1–4 (implemented; independent of the app)
producer(s) -> ThreadSafeQueue<T> -> consumer(s)
                 close() / drain
                 stop_token-local cancellation for waiters

post(task) -> SerialTaskWorker [owns std::jthread]
                     -> ThreadSafeQueue<Task> -> single worker callback
                          request_close() -> drain -> join
                 queue + worker + ownership CTests
                 standalone or canonical CTest (after guarded registration)

Phase 8 future (planned, not implemented)
actual Qt/PJSIP owner audit + 100k MPMC stress / TSan verification
                              -> carefully scoped integration if safe
```

## `ThreadSafeQueue<T>` contract (implemented)

- Unbounded FIFO guarded by one mutex and one condition variable.
- `push(T)` makes a value available on success, returns `false` if closed.
- `try_pop()` never blocks; empty returns `std::nullopt`.
- `wait_pop()` blocks until an item is present or the queue closes; drains
  remaining items even after close; once closed-and-empty, returns nullopt.
- `wait_pop(std::stop_token)` wakes on queued data, queue closure, or the
  calling worker's requested stop. It returns `std::nullopt` for cancellation
  **without consuming an item** when cancellation is observed before dequeue.
  If stop races with a successful dequeue, either outcome is permitted.
- A local stop request does **not** close the queue; other consumers and
  producers can continue using it.
- `close()` is idempotent, cannot be undone, and wakes all waiting consumers.
- `size()` and `is_closed()` are point-in-time observations only.
- Consumers share the queue; any one item is removed by at most one consumer.
- Inter-producer FIFO follows mutex acquisition/linearization order.
- Owner must join producers/consumers before destroying the queue.
- No PJSIP/Qt API calls are made from queue internals.

## Ownership and error behavior

The queue does not own its threads. Thread owners must stop producers,
choose whether to close the queue to drain outstanding work or request
per-consumer stop to abandon outstanding work, join all consumers, then
destroy the queue. Calling `request_stop()` on a consumer does not close the
queue or automatically stop producers. `std::condition_variable_any` enables
C++20 stop-token-aware waiting; an ordinary `wait_pop()` still requires a
push or `close()` to unblock. If `T` throws during
its move into an optional result, the queue entry is not popped; `T` may
nevertheless have been modified by a throwing move. Avoid throwing payload
moves in concurrency-sensitive paths.

## `SerialTaskWorker` contract (Commit 3; implemented in isolation)

- The owner constructs a worker thread, posts callable `std::function<void()>`
  tasks, and invokes `shutdown()` from outside the worker to close, drain, and
  join. Destruction also closes/drains/joins. A worker callback may safely
  invoke `request_close()` but not `shutdown()`.
- Only one callback executes at a time. Tasks accepted by the queue are
  executed exactly once, in queue order, before `shutdown()` returns.
  Concurrent producers have no wall-clock submission ordering guarantee.
- `post()` returns `false` for an empty callable or after close. As with any
  allocating queue, allocation failures can throw rather than returning
  `false`. There is no bounded backpressure or abort-pending-work mode yet.
- Task exceptions are caught at the worker boundary; `failures()` reports
  throwing callbacks and `completed()` counts all finished callbacks. The
  error payload is not retained. Application code must not mistake these
  counters for a UI error-handling policy.
- Concurrent external `shutdown()` calls serialize their join. Caller must
  still ensure that object destruction cannot race with active `post()`,
  `request_close()`, `shutdown()`, or callbacks capturing owner resources.
  Self-destruction from the worker callback is unsupported.
- Worker callbacks run on the owned C++ thread, **not** automatically on the
  Qt GUI thread or the PJSIP endpoint-owner thread. Never access Qt UI state
  or PJSIP objects through this worker without an independently verified
  ownership and queued-handoff contract.

## Commit 4 test ownership and CTest boundary

- The ownership test exercises four concurrent *external* task producers
  and asserts that their callbacks execute only on the `SerialTaskWorker`
  thread. A nested post from a running callback must execute later, after
  an already-queued task, and on that same owned thread. These checks are
  not a general replacement for race detection or the planned MPMC stress.
- Standalone `tests/phase8` retains a separately configurable CMake project.
  When entered from the canonical parent, the file avoids a nested project
  and registers three `Phase8*` CTest entries. The guarded integration
  script appends `add_subdirectory(phase8)` to `tests/CMakeLists.txt` in the
  canonical checkout. It does not edit Qt or PJSIP runtime code.
- The app-level ownership/callback audit is still pending; nothing here
  authorizes PJSUA2 access from the general-purpose worker thread.

## Future acceptance checks

Before integrating the queue into application workflows, establish the
actual GUI/PJSUA2 callback context and verify ordering, cancellation,
`std::jthread` teardown, account lifetime, post-ring rejection semantics,
GUI queued signal delivery, and RTP diagnostic updates. Use a separate TSan
build; do not combine TSan with ASan in one executable.
