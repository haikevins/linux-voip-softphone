# Phase 8 — Multithreading

## Status

**Completed** — milestone `v0.9-threading`.

Phase 8 moves SIP backend construction, command execution, and destruction off the Qt UI thread through a dedicated worker while preserving the Phase 7 QML/ViewModel/application boundary. Worker lifecycle, queue behavior, the 100,000-event stress case, full regression, and the CI ThreadSanitizer gate passed the documented automated checks.

## Objective

Move SIP backend work off the Qt UI thread, make thread ownership explicit, add reusable concurrency primitives, and verify lifecycle/queue behavior under stress without changing the Phase 7 QML/ViewModel/application boundary.

## Scope

Included in this phase:

- a dedicated serialized `WorkerExecutor` with FIFO task dispatch;
- asynchronous SIP command execution through `ThreadedSipEngine`;
- construction, command execution, and destruction of the concrete SIP backend on the worker thread;
- mutex-protected forwarding of backend callbacks to the existing `SipEngineObserver` boundary;
- Qt composition that wraps the real or unavailable SIP backend in `ThreadedSipEngine`;
- queued Qt event-loop refresh through the existing `CallViewModel::on_state_changed()` path;
- a reusable `ThreadSafeQueue<T>` based on `std::mutex` and `std::condition_variable`;
- lifecycle tests for worker ownership, command draining, and backend destruction order;
- a 100,000-event multi-producer/multi-consumer queue stress test with atomic accounting;
- a ThreadSanitizer build preset and CI runtime gate;
- one consolidated Phase 8 verification entry point.

Deliberately excluded from this phase:

- updating QML objects directly from SIP/PJSIP callback threads;
- adding independent metrics or logging worker threads from the roadmap sketch;
- replacing the existing `CallManager` state machine or Phase 7 UI behavior;
- multi-call/conference orchestration;
- treating the standalone `ThreadSafeQueue<T>` as the SIP command queue when the production path actually uses `WorkerExecutor`'s private task queue;
- claiming local ThreadSanitizer runtime coverage on hosts where the runtime cannot initialize.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| Worker executor | Serialize application tasks on one dedicated worker | `src/threading/WorkerExecutor.*` |
| Threaded SIP adapter | Own and call the concrete `SipEngine` on the worker thread | `src/application/ThreadedSipEngine.*` |
| Generic queue | Provide blocking close/drain producer-consumer semantics | `src/threading/ThreadSafeQueue.hpp` |
| GUI composition | Wrap the Qt session backend in the threaded adapter | `src/gui/main.cpp` |
| Worker tests | Verify FIFO dispatch, failure propagation, shutdown, and rejection rules | `tests/unit/test_worker_executor.cpp` |
| SIP-thread tests | Verify backend thread ownership, callback forwarding, failures, and destruction order | `tests/unit/test_threaded_sip_engine.cpp` |
| Queue tests | Verify FIFO, blocking wakeup, close behavior, and move-only values | `tests/unit/test_thread_safe_queue.cpp` |
| Stress test | Transfer 100,000 events exactly once across multiple producers/consumers | `tests/stress/thread_safe_queue_stress.cpp` |
| Sanitizer support | Build and run ThreadSanitizer where the host allows it | `CMakePresets.json`, `cmake/Sanitizers.cmake`, `.github/workflows/test.yml` |
| Phase verifier | Run inherited verification, Phase 8 tests, regression, and TSan build | `scripts/phase8/verify.sh` |
| Architecture note | Explain ownership, execution contexts, queues, callbacks, and shutdown | `docs/architecture/threading-model.md` |
| Interview review | Review concurrency choices, trade-offs, and limitations | `docs/interview/phase8-questions.md` |

## Architecture and design

The production GUI path is now:

```text
Qt UI thread
    |
    v
QML -> CallViewModel -> CallManager -> SipEngine
                                      |
                                      v
                              ThreadedSipEngine
                                      |
                         WorkerExecutor task queue
                                      |
                                      v
                              dedicated worker
                                      |
                                      v
                               Pjsua2SipEngine
                                      |
                              PJSUA2 / SIP / RTP
```

`src/gui/main.cpp` remains the composition root. It builds a concrete backend factory and passes that factory to `ThreadedSipEngine`. The adapter constructs the backend on its worker, queues SIP commands onto that worker, and destroys the backend there during teardown.

Callbacks travel in the opposite direction:

```text
PJSUA2/backend callback
    -> ThreadedSipEngine
    -> SipEngineObserver / CallManager
    -> CallStateMachine
    -> StateChangeSink / CallViewModel
    -> Qt::QueuedConnection
    -> Qt-property refresh on the ViewModel event loop
```

`ThreadSafeQueue<T>` is a separate reusable producer-consumer primitive. The production SIP command path uses `WorkerExecutor`'s private `std::deque<std::packaged_task<void()>>` protected by its own mutex/condition variable; the generic queue is verified independently with unit and stress tests.

See `docs/architecture/threading-model.md` for the detailed thread ownership and shutdown model.

## Build and run

The complete Phase 8 verifier inherits the Phase 7 Qt/PJSIP gate, so expose the local PJPROJECT 2.17 installation first:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase8/verify.sh
```

The normal verifier builds the ThreadSanitizer configuration but does not force local runtime execution. On a host where ThreadSanitizer can initialize, run:

```bash
RUN_TSAN_TESTS=1 ./scripts/phase8/verify.sh
```

The GUI itself is still built and launched through the Qt+PJSIP preset:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake --preset qt-pjsip-debug
cmake --build --preset qt-pjsip-debug -j
./build/qt-pjsip-debug/src/gui/linux-voip-softphone-gui
```

## Verification

The Phase 8 automated gate covers:

- inherited Phase 7 build/test/GUI verification;
- repository diff checks;
- fresh non-PJSIP debug build;
- the Phase 8-labeled worker, threaded-SIP, queue, and stress tests;
- full non-PJSIP regression;
- fresh ThreadSanitizer build;
- optional local runtime TSan execution through `RUN_TSAN_TESTS=1`.

The accepted pre-documentation run reported 17 Phase 8 tests and 94 total non-PJSIP tests. Those counts are observations from that run rather than permanent contract values; `scripts/phase8/verify.sh` enforces a minimum Phase 8 count instead of hard-coding the full suite size.

The stress test sends 100,000 integer event IDs through four producer threads and four consumer threads. Atomic counters verify that every event is consumed exactly once without relying on unsynchronized shared accounting.

The development host used for this phase can build ThreadSanitizer binaries but may fail to start TSan-instrumented tests with an `unexpected memory mapping` error. Runtime TSan therefore remains an authoritative CI gate when the local host cannot support it.

## Evidence

The accepted Phase 8 evidence set is:

```text
evidence/phase8/
├── README.md
├── automated-verification.txt
├── stress-test.txt
└── tsan-report.txt
```

The summaries record the complete Phase 8 verifier, the 100,000-event producer/consumer stress test, and the distinction between local TSan build coverage and CI runtime TSan coverage. They intentionally avoid raw SIP/authentication traces.

## Interview focus

Use `docs/interview/phase8-questions.md` together with the implementation. Be ready to explain:

- why SIP backend construction/destruction belongs on the worker thread;
- why `WorkerExecutor` serializes commands instead of spawning a thread per action;
- the difference between the production worker task queue and the reusable `ThreadSafeQueue<T>`;
- how condition variables avoid polling;
- why callback forwarding and Qt UI refresh are separate synchronization boundaries;
- what graceful shutdown means for queued commands and backend lifetime;
- what the 100,000-event stress test proves and does not prove;
- why atomics are used for stress-test accounting;
- what ThreadSanitizer can detect and why a successful build is not the same as a successful runtime TSan test.

## Exit criteria

Phase 8 is ready for milestone closure when:

- the GUI composition uses `ThreadedSipEngine` rather than directly owning the concrete SIP backend;
- backend construction, SIP commands, and backend destruction execute on the worker thread;
- queued work drains deterministically during shutdown;
- Qt-facing refresh remains queued through the ViewModel event loop;
- `ThreadSafeQueue<T>` passes unit tests and the 100,000-event producer/consumer stress test;
- the complete Phase 8 verifier passes on the documented environment;
- ThreadSanitizer runtime verification passes in CI or another supported host;
- architecture, interview review, and sanitized Phase 8 evidence are present.

The recorded automated verification, queue/lifecycle tests, stress test, CI ThreadSanitizer runtime gate, documentation, and sanitized evidence satisfy these exit criteria. The `v0.9-threading` tag is created only after review, merge to `main`, and verification of the merged milestone state.

## References

- Architecture: `docs/architecture/threading-model.md`
- Prior GUI architecture: `docs/architecture/qt-qml-gui.md`
- Call-state architecture: `docs/architecture/call-state-machine.md`
- Interview review: `docs/interview/phase8-questions.md`
- Phase verifier: `scripts/phase8/verify.sh`
- Worker implementation: `src/threading/WorkerExecutor.*`
- Threaded SIP adapter: `src/application/ThreadedSipEngine.*`
- Generic queue: `src/threading/ThreadSafeQueue.hpp`
- Tests: `tests/unit/test_worker_executor.cpp`, `tests/unit/test_threaded_sip_engine.cpp`, `tests/unit/test_thread_safe_queue.cpp`, `tests/stress/thread_safe_queue_stress.cpp`

## Next phase

Phase 9 focuses on debugging, memory diagnostics, GDB/core-dump analysis, and sanitizer-driven failure investigation without weakening the threading boundaries established here.
