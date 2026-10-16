# Phase 8 Interview Review — Multithreading and Concurrency

## Review scope

This review covers the Phase 8 execution model: moving SIP backend ownership off the Qt UI thread, serialized worker dispatch, callback handoff, mutex/condition-variable usage, generic producer-consumer queues, deterministic shutdown, stress testing, atomics, and ThreadSanitizer.

## Questions and answers

### Thread ownership

#### What problem does Phase 8 solve that Phase 7 did not?

Phase 7 already protected `CallManager` state with a mutex and queued Qt-property refreshes, but the GUI composition still constructed and called the concrete SIP backend directly through the application path.

Phase 8 adds an explicit worker boundary. `ThreadedSipEngine` owns a dedicated `WorkerExecutor`, constructs the concrete backend on that worker, and dispatches SIP commands there. This makes backend ownership and blocking-work placement explicit instead of relying only on mutexes around shared state.

#### What is the current production thread model?

At the application-owned level there is the Qt UI thread plus one dedicated SIP worker:

```text
Qt UI thread
    -> CallViewModel
    -> CallManager
    -> ThreadedSipEngine
    -> WorkerExecutor
    -> SIP worker
    -> Pjsua2SipEngine
```

PJSUA2 may also invoke callbacks from library-managed execution contexts. Those callbacks cross back through `SipEngineObserver` and eventually reach Qt through a queued ViewModel refresh.

The roadmap sketch also showed metrics and logging threads, but those are not implemented in Phase 8 and should not be claimed.

#### Why not create a new thread for every SIP command?

The backend represents one shared SIP session. Serializing commands through one worker preserves ordering and makes lifetime easier to reason about. A thread per command would allow overlapping calls into the same backend, increase teardown complexity, and provide no demonstrated benefit for the current one-call model.

#### Why create the concrete SIP backend on the worker instead of only moving method calls?

Thread ownership should cover the full object lifetime, not just selected methods. The concrete engine is created through a factory dispatched to the worker and destroyed by a cleanup task on that same worker.

That produces a clear rule: application-owned interaction with the concrete backend belongs to the worker execution context.

### WorkerExecutor

#### How does `WorkerExecutor` serialize tasks?

It stores `std::packaged_task<void()>` objects in a `std::deque` protected by a mutex. `dispatch()` pushes at the back, the worker pops from the front, and a condition variable wakes the worker when work arrives.

The mutex is released before executing the task, so a long SIP operation does not block other threads merely trying to enqueue work or inspect shutdown state.

#### Why use `std::packaged_task` and `std::future<void>`?

`std::packaged_task` captures task exceptions into a future rather than terminating the worker thread. The future is useful when construction or destruction must synchronously confirm completion.

Normal SIP commands intentionally dispatch asynchronously; `ThreadedSipEngine` catches backend exceptions inside those tasks and translates them into existing observer failure callbacks.

#### What happens during `WorkerExecutor::shutdown()`?

The executor stops accepting new work, wakes the worker, and lets the worker process already queued tasks until the queue becomes empty. The owning thread then joins the worker.

This is graceful draining rather than immediate cancellation.

#### Why reject dispatches after shutdown?

Accepting new work after shutdown begins makes completion unbounded and object lifetime ambiguous. Returning an error gives callers a deterministic lifecycle contract.

### ThreadSafeQueue and synchronization

#### Does the production SIP worker use `ThreadSafeQueue<T>`?

No. This distinction matters.

The production SIP worker uses `WorkerExecutor`'s private deque of packaged tasks because it needs executable tasks, futures, and executor lifecycle semantics.

`ThreadSafeQueue<T>` is a separate reusable value-oriented producer-consumer primitive. It is verified independently with unit tests and a 100,000-event stress test.

#### How does `ThreadSafeQueue<T>` avoid busy waiting?

`wait_and_pop()` waits on a condition variable with the predicate:

```text
closed OR queue not empty
```

A producer calls `notify_one()` after pushing. `close()` calls `notify_all()` so consumers blocked on an empty queue wake and can terminate once the queue is drained.

#### Why must the condition-variable wait use a predicate?

Condition variables may wake spuriously, and wakeups can occur after state changes. The predicate re-checks the actual protected condition under the mutex and prevents treating a wakeup itself as proof that data exists.

#### What are the queue close semantics?

After `close()`:

- new pushes are rejected;
- already queued values remain consumable;
- waiting consumers wake;
- `wait_and_pop()` returns `std::nullopt` only when the queue is closed and empty.

This supports deterministic producer-completion and consumer shutdown.

#### Why does the queue use a mutex instead of only atomics?

Its state is compound: the queue container, element movement, close flag, and condition-variable coordination must remain consistent together. Atomics are appropriate for independent scalar state but do not replace the mutex around the queue operation as a whole.

### Callback handoff and Qt

#### If SIP commands run on a worker, why is another Qt handoff still necessary?

Moving command execution off the UI thread does not mean callbacks automatically arrive on the Qt UI thread. The backend can invoke observer callbacks from its own execution context.

`CallManager` translates those callbacks into application state, and `CallViewModel::on_state_changed()` uses `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` so Qt-facing property refresh happens through the ViewModel's event loop.

#### What does the observer mutex inside `ThreadedSipEngine` protect?

It protects the external observer pointer while callbacks may race with observer removal during teardown. The destructor clears the observer under the same mutex before destroying the backend, preventing later forwarded callbacks from dereferencing a stale observer pointer.

#### Does one mutex protect the whole application?

No. Synchronization responsibilities are separated:

- `WorkerExecutor` protects its task queue/lifecycle;
- `ThreadedSipEngine` protects its observer pointer;
- `CallManager` protects application call state;
- Qt's event loop serializes the queued ViewModel refresh on the GUI side.

Using one global mutex would increase coupling and make lock ordering harder to reason about.

### Lifecycle and failure handling

#### How is backend destruction ordered relative to queued commands?

`ThreadedSipEngine` queues a destruction task onto the same FIFO worker after earlier accepted commands and waits for that cleanup future. The backend observer is detached and the engine is reset from the worker task.

A dedicated test verifies that an already queued command completes before backend destruction.

#### What happens if backend initialization fails?

Construction dispatches the backend factory and waits on its future. If the factory throws or returns null, construction fails immediately and no partially usable `ThreadedSipEngine` is exposed.

#### What happens if a SIP command throws on the worker?

Registration command failures are translated to `on_registration_failed()`. Call-control failures are translated to `on_call_failed()` with the operation name plus the exception message when available.

This keeps the asynchronous boundary compatible with the existing application failure model.

### Stress testing and ThreadSanitizer

#### What does the 100,000-event stress test verify?

It runs four producers and four consumers through `ThreadSafeQueue<int>`. Each produced event ID should be consumed exactly once. Atomic accounting detects missing or duplicate consumption without adding an unsynchronized counter race to the test itself.

It exercises concurrent queue behavior at higher volume than the focused unit tests.

#### Does a passing stress test prove the code is race-free?

No. Stress tests improve the chance of exposing timing-sensitive defects but cannot enumerate every possible schedule.

That is why Phase 8 also uses deterministic lifecycle tests and ThreadSanitizer.

#### What does ThreadSanitizer add?

TSan instruments memory accesses and synchronization at runtime to detect data races that may not manifest as visible failures in normal tests.

A TSan build alone does not provide this dynamic check; the instrumented tests must actually run successfully.

#### Why can local TSan be skipped while CI remains required?

The documented development host can build TSan binaries but may fail during TSan runtime startup with an `unexpected memory mapping` error before project tests execute. That is a host/runtime limitation rather than a passing race check.

`scripts/phase8/verify.sh` therefore always verifies a fresh TSan build and runs TSan tests locally only with `RUN_TSAN_TESTS=1`. CI is the required runtime TSan gate for the accepted milestone.

### Trade-offs and phase boundary

#### Why did Phase 8 not implement separate SIP, metrics, and logging threads exactly like the roadmap sketch?

The current requirement is to keep SIP backend work away from the Qt UI thread and prove concurrency fundamentals. One dedicated SIP worker solves that concrete ownership problem.

Creating metrics/logging threads without actual independent workloads would add threads for portfolio appearance rather than engineering need. They remain outside the implemented Phase 8 boundary.

#### Why is `ThreadSafeQueue<T>` not wired into `WorkerExecutor` immediately?

The generic queue moves values and exposes close/drain semantics. The executor additionally needs callable task storage, futures, explicit task validation, exception capture, and worker shutdown behavior.

Keeping the abstractions separate avoids forcing executor-specific behavior into the generic queue. A later refactor could compose them if a clear benefit appears and tests preserve the same lifecycle contracts.

#### What is the strongest Phase 8 point to explain in an interview?

The important point is not simply "I used mutexes." It is that the project defines explicit ownership and handoff boundaries:

```text
Qt actions
    -> serialized worker commands
    -> worker-owned SIP backend

backend callbacks
    -> application state
    -> queued Qt refresh
```

The design is backed by lifecycle tests, a generic producer-consumer queue, a 100,000-event stress test, atomics for independent accounting, and runtime race detection in CI.

## Phase boundary

Phase 8 proves explicit SIP worker ownership, serialized command dispatch, deterministic backend lifetime, safe observer forwarding, queued Qt refresh, reusable queue synchronization, stress verification, and ThreadSanitizer integration.

It does not claim separate metrics/logging workers, a general executor pool, lock-free structures, multi-call concurrency, or universal local TSan runtime support.

## References

- `docs/phase8/README.md`
- `docs/architecture/threading-model.md`
- `docs/architecture/qt-qml-gui.md`
- `src/threading/WorkerExecutor.*`
- `src/threading/ThreadSafeQueue.hpp`
- `src/application/ThreadedSipEngine.*`
- `src/gui/main.cpp`
- `src/gui/CallViewModel.cpp`
- `tests/unit/test_worker_executor.cpp`
- `tests/unit/test_threaded_sip_engine.cpp`
- `tests/unit/test_thread_safe_queue.cpp`
- `tests/stress/thread_safe_queue_stress.cpp`
- `scripts/phase8/verify.sh`
