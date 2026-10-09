# Threading model — Phase 8

## Provenance and current baseline

According to the Phase 7 handoff for `v0.8-qt-gui`, the GUI backend already
uses `std::jthread`, mutex/condition-variable synchronization, and Qt queued
handoff to the GUI. `CallManager` has an owner-thread contract. These are
**existing boundaries**; this document does not assert that a fresh runtime
trace or code audit of the canonical clone was completed for Commit 1.

The queue introduced in Phase 8 Commit 1 and extended in Commit 2 is an
**isolated library component**. It is not wired to the GUI, `CallManager`, or
PJSUA2 yet. Commit 1's standalone tests were also run in the canonical
ThinkPad repository on 2026-10-09 (reported by the maintainer).

## Current vs. planned event paths

```text
Phase 7 baseline (reported in release handoff)
PJSUA2/session worker -> Qt queued handoff -> GUI state/UI thread
                  CallManager: owner-thread affinity

Phase 8 Commits 1–2 (implemented)
producer(s) -> ThreadSafeQueue<T> -> consumer(s)
                 close() / drain
                 stop_token-local cancellation for waiters
                 standalone CTest

Phase 8 future (planned, not implemented)
worker/event adapter -> owning thread + 100k MPMC stress / TSan verification
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

## Future acceptance checks

Before integrating the queue into application workflows, establish the
actual GUI/PJSUA2 callback context and verify ordering, cancellation,
`std::jthread` teardown, account lifetime, post-ring rejection semantics,
GUI queued signal delivery, and RTP diagnostic updates. Use a separate TSan
build; do not combine TSan with ASan in one executable.
