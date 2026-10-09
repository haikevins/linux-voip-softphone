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
Commits 1–5 were tested in the canonical ThinkPad repository on
2026-10-09 (maintainer-supplied terminal logs); Commit 4 recorded 138/138
canonical CTest entries at `c7a4243`, and Commit 5 recorded 139/139 plus
100 stress repetitions at `5979869`. Nothing here establishes Qt/PJSIP
callback thread affinity.

## Current vs. planned event paths

```text
Phase 7 baseline (reported in release handoff)
PJSUA2/session worker -> Qt queued handoff -> GUI state/UI thread
                  CallManager: owner-thread affinity

Phase 8 Commits 1–5 (implemented; independent of the app)
producer(s) -> ThreadSafeQueue<T> -> consumer(s)
                 close() / drain
                 stop_token-local cancellation for waiters

post(task) -> SerialTaskWorker [owns std::jthread]
                     -> ThreadSafeQueue<Task> -> single worker callback
                          request_close() -> drain -> join
                 queue + worker + ownership + 100k MPMC stress CTests
                 standalone or canonical CTest (registered in Commit 4)

Phase 8 Commit 6 (standalone only)
-fsanitize=thread -> 4 Phase 8 test executables (not Qt/PJSIP)

Phase 8 future (planned, not implemented)
actual Qt/PJSIP owner audit
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

## Commit 5: contention, shutdown accounting, and exception injection

- Four concurrent producers enqueue **100,000 unique events**, and four
  consumers dequeue them. Atomic per-ID occurrence counters verify exact-once
  delivery without introducing test-side data races if the queue misbehaves.
  Consumers drain after producers join and the queue is closed.
- A four-producer/one-consumer test separately asserts per-producer FIFO.
  With multiple consumers, post-dequeue scheduling does not establish the
  queue's dequeue serialization order, so it is deliberately **not** used as
  an ordering oracle in the 4x4 test.
- In the close-contended scenario, accepted/rejected counts can vary with
  scheduling; the invariant is that every accepted ID is delivered once,
  no event is delivered twice, and post-close pushes are rejected. `close()`
  remains a drain rather than a data-drop operation.
- A separate 10,000-task worker scenario injects deterministic exceptions;
  `completed()` counts every accepted callback and `failures()` counts all
  injected exceptions. These are generic concurrency tests, not SIP/Qt
  runtime ownership evidence. Commit 5 was verified on the ThinkPad:
  100/100 stress repeats, canonical 139/139, push `5979869` synchronized.

## Commit 6: TSan scope and coverage

- TSan is **opt-in** via `PHASE8_ENABLE_TSAN=ON` for a standalone
  `tests/phase8` build, never by instrumenting only one portion of Qt/PJSIP.
  All four standalone test translation units and their header-only queue /
  worker implementations compile and link with `-fsanitize=thread`.
- `scripts/phase8/verify-tsan.sh` uses its own build tree and runs CTest
  serially. A failure or race report fails the gate; unsupported runtime
  mappings are recorded as BLOCKED rather than passed.
- Do not mix TSan with ASan/UBSan or reuse their build directories. Normal
  canonical CTest builds leave TSan off, preserving the 139-entry baseline.
- The isolated Linux GCC 14.2 TSan run completed four tests three times each
  with no reported races. This cannot establish race freedom for unexercised
  interleavings or any Qt/PJSIP callbacks. On the ThinkPad, GCC 13 TSan
  was BLOCKED by memory mapping failures. Clang 18.1.3 successfully
  completed four instrumented tests three times each. These results
  are recorded separately in `evidence/phase8/tsan-report.txt`.

## Future acceptance checks

Before integrating the queue into application workflows, establish the
actual GUI/PJSUA2 callback context and verify ordering, cancellation,
`std::jthread` teardown, account lifetime, post-ring rejection semantics,
GUI queued signal delivery, and RTP diagnostic updates. Use a separate TSan
build; do not combine TSan with ASan in one executable.

## Commit 8 source-review update (runtime validation pending)

Static inspection of canonical `cf31e2a` identifies Qt queued worker-to-GUI
handoffs and a joined registration-session worker. It also confirms PJSUA2
callback overrides perform library operations, so "all PJSUA2 operations run
on the Qt session worker" must not be treated as a verified invariant.
`SipOutgoingCallClient::stop()` and the independent incoming client release
the call object before waiting for zero active call slots. This is a
**lifetime verification risk**, not an observed data race or UAF. See
`docs/phase8/commit8-source-ownership-review.md`. A Qt-only fake-session
lifetime regression is pending canonical ThinkPad verification.

## Commit 10 — bounded PJSIP call-slot polling (not callback quiescence)

The two PJSIP clients now share a deterministic, injected polling policy
for the existing 10ms/2s call-slot wait. Six direct C++ cases exercise its
success and timeout decisions. `pjsua_call_get_count() == 0` is only an
observed call-slot count, **not proof of callback thread quiescence**. On
`shutdown2(force=false)` failure, account cleanup remains an open risk.
Do not infer runtime callback lifetime correctness or broad PJSIP TSan coverage
from the added standalone fifth CTest.
