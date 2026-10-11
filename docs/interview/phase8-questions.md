# Phase 8 Interview Review — Multithreading

## Review scope

This review covers the concurrency work actually implemented and evidenced in
Phase 8: `ThreadSafeQueue<T>`, `SerialTaskWorker`, MPMC stress, standalone
ThreadSanitizer, Qt owner-thread handoff, and the PJSUA2 cleanup/shutdown
lifetime boundary.

It deliberately does not claim that the generic queue replaced Qt queued
connections, that vendor PJSUA2 callbacks are proven quiescent at shutdown, or
that live SIP teardown is universally safe.

## Questions and answers

### Queue and worker design

#### Why introduce a generic `ThreadSafeQueue<T>` if Qt already has queued connections?

The generic queue makes C++20 producer/consumer semantics independently
executable and testable: close/drain behavior, local cancellation, MPMC
integrity, and ownership are verified without requiring Qt or PJSUA2. It is a
reusable concurrency primitive, not evidence that the production Qt path was
rewired to use it.

#### What is the difference between a consumer stop request and `close()`?

A stop token cancels one waiting consumer. It does not reject producers or
close the shared queue. `close()` is a queue-wide lifecycle event: future
pushes are rejected and waiting consumers wake so already accepted work can be
drained.

#### What ordering does the queue guarantee with multiple producers?

Each successful push is serialized by the queue mutex. FIFO is the
linearization order of those successful pushes, not a guarantee based on
wall-clock call order across independent producer threads. Separate tests use
one consumer when checking per-producer FIFO.

#### Why does `SerialTaskWorker` drain instead of discarding queued tasks?

The worker treats accepted tasks as work that must be accounted for before
shutdown completes. Closing the queue and joining after drain provides a
simple deterministic contract. Phase 8 does not implement bounded
backpressure or an abort-pending-work mode.

#### Why can a worker callback request close but not call `shutdown()`?

`shutdown()` joins the owned thread. Calling it from that same worker would be
a self-join. `request_close()` safely stops accepting new work without trying
to join from inside the callback.

### Verification and sanitizer scope

#### What does the 100,000-event stress test prove?

For the observed executions it verifies that four producers and four consumers
can transfer 100,000 distinct IDs without missing, duplicate, malformed, or
unexpectedly rejected items, and that close/drain accounting holds. It does
not prove every possible scheduler interleaving or anything about live
PJSUA2/Qt callback behavior.

#### Why is a passing TSan run not proof that the softphone is race-free?

The recorded Clang 18 TSan gate instruments the standalone Phase 8 C++ tests,
not the Qt GUI executable or vendor PJSUA2 runtime. It gives strong evidence
for the exercised queue/worker/model code but cannot cover components that
were not instrumented or executed.

#### Why was GCC 13 TSan recorded as BLOCKED instead of PASS?

The ThinkPad GCC 13 runtime failed because of sanitizer memory-mapping issues,
including a minimal reproduction. Treating an environment where the
sanitizer cannot execute as race-free would be false evidence. Clang 18 was
used for the successful standalone TSan verification instead.

### Qt and PJSUA2 ownership

#### How does the production GUI path return work to the UI owner thread?

`QtRegistrationBackend` owns a session `std::jthread` and publishes
registration/call/RTP events using `QMetaObject::invokeMethod(...,
Qt::QueuedConnection)`. Those callbacks execute on the QObject/UI side before
updating `GuiViewModel` / `CallManager` state. The generic Phase 8 worker does
not replace this path.

#### Why is `pjsua_call_get_count() == 0` not sufficient shutdown evidence?

It reports observed PJSUA call-slot state. It does not prove that all external
or in-flight vendor callbacks referencing account/call-owned objects have
completed. Phase 8 therefore treats call-slot cleanup and callback quiescence
as separate properties.

#### What is the purpose of the shutdown authorization helper?

It makes the policy decision explicit: destruction/integration is authorized
only when the modeled preconditions establish a safe enough state. If
shutdown failure, stale results, external callback activity, or unresolved
ownership make safety uncertain, the contract fails closed instead of
assuming teardown is safe.

#### Why can the roadmap exit audit PASS while integration readiness is BLOCKED?

They answer different questions. The exit audit can confirm that required
source, tests, documentation, and evidence are present and internally
consistent. The integration audit separately reports whether production
PJSUA2 shutdown has enough evidence to authorize a stronger safety claim.
Phase 8 intentionally preserves `BLOCKED/UNPROVEN` for the latter.

### Phase boundary

#### What is the strongest defensible Phase 8 conclusion?

The project has a tested C++20 concurrency foundation, repeated MPMC stress,
standalone TSan coverage, explicit Qt owner-thread handoff documentation, and
fail-closed PJSUA2 lifetime/shutdown models. Final offline GUI acceptance also
passed. It is not defensible to claim that live PJSUA2 teardown is universally
safe or that the generic queue is deployed as the Qt/PJSUA2 event transport.

## Phase boundary

Phase 8 can be reviewed as an offline/educational multithreading milestone only
with the remaining PJSUA2 lifetime risks carried explicitly. Live external
callback quiescence, actual `PJ_EBUSY` recovery, and safe teardown after every
shutdown failure remain unproven unless later evidence resolves them.

Phase 9 should start only after Phase 8 is explicitly accepted, merged, and
released according to the repository workflow.

## References

- `docs/phase8/README.md`
- `docs/architecture/threading-model.md`
- `docs/phase8/roadmap-exit-review.md`
- `docs/phase8/final-runtime-acceptance.md`
- `evidence/phase8/README.md`
- `evidence/phase8/roadmap-closure.md`
- `tests/unit/` and `tests/integration/`; focused CMake harness: `cmake/phase8-focused/`
- `scripts/phase8/`
