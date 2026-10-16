# Phase 8 Architecture — Threading and Concurrency

## Status

Completed — milestone `v0.9-threading`.

## Purpose

Define explicit execution contexts for the Qt GUI and SIP backend so potentially blocking or backend-owned SIP work does not execute directly on the Qt UI thread, while preserving deterministic state transitions and teardown.

## Scope

This architecture covers:

- Qt UI-thread responsibilities;
- `WorkerExecutor` ownership and task serialization;
- `ThreadedSipEngine` backend lifetime and command dispatch;
- observer callback forwarding into `CallManager`;
- the queued ViewModel refresh back into Qt's event loop;
- generic `ThreadSafeQueue<T>` producer-consumer semantics;
- shutdown and queued-work draining;
- concurrency verification through unit tests, stress tests, atomics, and ThreadSanitizer.

It does not define independent metrics/logging worker threads, a general thread pool, multi-call scheduling, or a lock-free queue.

## Architecture model

### Production execution contexts

The current GUI runtime has two primary application-owned execution contexts:

```text
Qt UI thread
    |
    | user actions / ViewModel calls
    v
CallViewModel -> CallManager -> SipEngine
                              |
                              v
                      ThreadedSipEngine
                              |
                              | dispatch packaged task
                              v
                      WorkerExecutor queue
                              |
                              v
                   dedicated SIP worker thread
                              |
                              v
                       concrete SipEngine
                     (Pjsua2SipEngine)
```

`ThreadedSipEngine` is still a `SipEngine`, so neither `CallManager` nor QML needs to know about `std::thread`, PJSUA2 worker ownership, or the concrete backend type.

### Ownership

`ThreadedSipEngine` owns:

- one `WorkerExecutor`;
- one concrete `SipEngine` instance;
- one mutex-protected observer pointer.

The concrete engine is created by a factory inside a task submitted to `WorkerExecutor`. The constructor waits for that initialization future so the adapter is not exposed before its backend exists.

`src/gui/main.cpp` is the composition root:

```text
SipConnectionSettings
    -> concrete backend factory
    -> ThreadedSipEngine
    -> CallViewModel / CallManager
```

The Qt-only build follows the same threaded wrapper and places `UnavailableSipEngine` behind it. The Qt+PJSIP build creates `Pjsua2SipEngine` behind the same boundary.

### Command queue

`WorkerExecutor` owns a private:

```text
std::deque<std::packaged_task<void()>>
```

protected by a mutex and condition variable. `dispatch()` validates the task, creates a `std::packaged_task`, stores it at the back of the deque, wakes the worker, and returns a `std::future<void>` for optional completion/error observation.

The worker waits until work exists or shutdown has begun, removes from the front, unlocks the queue mutex, and then executes the task. This gives FIFO serialized command execution without holding the queue lock while user/backend code runs.

### Generic thread-safe queue

`ThreadSafeQueue<T>` is a reusable producer-consumer primitive implemented separately from `WorkerExecutor`:

```text
push(value)
    -> lock
    -> reject if closed
    -> enqueue
    -> unlock
    -> notify one

wait_and_pop()
    -> wait until closed or non-empty
    -> pop one value when available
    -> return nullopt only when closed and drained
```

`close()` prevents new pushes and wakes all waiting consumers. Existing queued values can still be drained.

This queue is not currently the production SIP command container. The production adapter uses `WorkerExecutor`'s packaged-task deque; the generic queue exists as a reusable concurrency primitive and is stress-tested independently.

### Callback path

Backend callbacks do not update QML objects directly:

```text
PJSUA2 / concrete backend callback
    -> ThreadedSipEngine::SipEngineObserver callback
    -> mutex-protected observer forwarding
    -> CallManager
    -> CallStateMachine transition
    -> StateChangeSink
    -> CallViewModel::on_state_changed()
    -> QMetaObject::invokeMethod(..., Qt::QueuedConnection)
    -> refresh_from_manager()
    -> Qt property signals / QML
```

`ThreadedSipEngine`'s observer mutex protects the observer pointer and prevents callback forwarding from racing with observer removal during destruction. `CallManager` keeps its own state mutex for application state. `CallViewModel` then uses a queued Qt invocation rather than refreshing Qt-facing state synchronously from the callback execution context.

## Runtime and lifecycle

### Construction

The sequence is:

```text
Qt composition root
    -> construct ThreadedSipEngine
    -> WorkerExecutor starts worker
    -> dispatch backend factory
    -> worker constructs concrete SipEngine
    -> worker sets ThreadedSipEngine as backend observer
    -> constructor waits for initialization future
    -> adapter becomes usable
```

Creating the concrete backend on the worker matters because backend initialization may establish thread-affine state or perform work that should not be owned by the Qt UI execution context.

### Commands

Registration and call commands are submitted asynchronously:

```text
CallManager action
    -> ThreadedSipEngine method
    -> WorkerExecutor::dispatch()
    -> return to caller
    -> worker invokes concrete backend
```

Backend exceptions are caught inside the dispatched command. Registration failures are translated to `on_registration_failed()`, while call-control failures are translated to `on_call_failed()`. They do not escape later from an unobserved asynchronous task into QML.

### Shutdown

`ThreadedSipEngine` teardown performs these steps:

```text
1. clear external observer under observer mutex
2. dispatch backend cleanup to worker
3. wait for backend cleanup future
4. remove backend observer and destroy backend on worker
5. stop accepting worker tasks
6. wake worker and drain previously queued tasks
7. join worker from non-worker context
```

Because backend cleanup is itself queued after earlier commands, FIFO ordering makes earlier accepted commands run before backend destruction. Tests explicitly verify this ordering.

`WorkerExecutor::shutdown()` is idempotent with respect to setting `accepting_ = false` and joins only when called outside the worker thread. New dispatches after shutdown are rejected.

### Condition variables and polling

Both concurrency primitives use condition variables so idle threads sleep instead of spinning. The wait predicates always include the state that permits shutdown, preventing an idle worker/consumer from sleeping forever after closure.

## Verification strategy

The concurrency model is verified in layers:

- `WorkerExecutorTest` checks dedicated-thread execution, FIFO ordering, exception capture, graceful drain, rejection after shutdown, and empty-task rejection.
- `ThreadedSipEngineTest` checks backend construction/destruction on the worker, command and callback execution context, failure translation, empty-factory rejection, and command-before-destruction ordering.
- `ThreadSafeQueueTest` checks FIFO behavior, blocking wakeup, close wakeup, push rejection after close, and move-only payload support.
- `ThreadSafeQueueStressTest` transfers 100,000 event IDs through four producers and four consumers.
- atomic counters in the stress test verify exactly-once accounting without introducing a second unsynchronized data race in the test itself.
- the `thread-sanitizer` preset instruments the Phase 8 binaries with TSan.
- GitHub CI is the required runtime TSan gate when the local host cannot initialize ThreadSanitizer.

A stress test can expose timing-sensitive mistakes but cannot prove the absence of all races. TSan adds dynamic race detection, while deterministic unit tests cover specific lifecycle contracts.

## Design rationale

### Why one serialized SIP worker?

The current softphone controls one active call and one backend instance. A single serialized worker makes ordering and backend lifetime easy to reason about and avoids concurrent calls into the same backend from multiple application-owned threads.

A thread pool would add scheduling complexity without a demonstrated Phase 8 need.

### Why keep the `SipEngine` interface?

Threading remains an implementation detail behind the application boundary. `CallManager` continues to depend on `SipEngine`, so the controller does not gain thread-management code and existing fake-engine tests remain useful.

### Why construct and destroy the backend on the same worker?

Moving only method calls would leave initialization/destruction ownership ambiguous. Creating, using, and destroying the concrete backend in the same execution context gives a stronger lifetime rule and makes teardown ordering testable.

### Why not update Qt directly from the worker/backend callback?

Qt GUI objects belong to the GUI event-loop context. The existing ViewModel queues `refresh_from_manager()` with `Qt::QueuedConnection`, so callback handling and Qt-property updates remain separated.

### Why have `ThreadSafeQueue<T>` when `WorkerExecutor` has its own queue?

They serve different interfaces:

- `WorkerExecutor` needs `std::packaged_task<void()>` plus futures and worker lifecycle.
- `ThreadSafeQueue<T>` demonstrates a reusable value-oriented producer-consumer abstraction with close/drain behavior.

Combining them prematurely would couple a generic queue API to executor-specific completion semantics. The duplication is deliberate and documented rather than hidden.

### Why atomics only in the stress-test accounting?

The queue's compound state still requires a mutex: enqueue/dequeue, close state, and condition-variable coordination are not a single scalar operation. Atomics are appropriate for independent counters used to record produced/consumed/duplicate observations in the stress test.

## Phase boundary

Phase 8 establishes one dedicated application-owned SIP worker, serialized SIP command dispatch, worker-owned backend lifetime, safe observer handoff, queued Qt refresh, reusable producer-consumer queue semantics, stress verification, and ThreadSanitizer coverage.

It does not implement the roadmap sketch's separate metrics/logging threads, a general thread pool, lock-free structures, or multi-call concurrency. Those should be added only when a later requirement justifies them.

## References

- `docs/phase8/README.md`
- `docs/architecture/qt-qml-gui.md`
- `docs/architecture/call-state-machine.md`
- `docs/interview/phase8-questions.md`
- `src/threading/WorkerExecutor.hpp`
- `src/threading/WorkerExecutor.cpp`
- `src/threading/ThreadSafeQueue.hpp`
- `src/application/ThreadedSipEngine.hpp`
- `src/application/ThreadedSipEngine.cpp`
- `src/gui/main.cpp`
- `src/gui/CallViewModel.cpp`
- `tests/unit/test_worker_executor.cpp`
- `tests/unit/test_threaded_sip_engine.cpp`
- `tests/unit/test_thread_safe_queue.cpp`
- `tests/stress/thread_safe_queue_stress.cpp`
- `scripts/phase8/verify.sh`
