# Phase 8 Interview Review — Multithreading

Use the repository implementation, documentation, tests, and evidence as the basis for the answers. Explain the engineering decision and the observed behavior; do not memorize isolated one-line definitions.

## Review scope

- `ThreadSafeQueue<T>` semantics and ownership
- `SerialTaskWorker` lifecycle
- repeated MPMC stress verification
- standalone ThreadSanitizer scope
- Qt owner-thread handoff
- PJSUA2 callback/lifetime boundaries
- fail-closed shutdown authorization
- accepted and unresolved Phase 8 claims

## Questions and answers

### Queue and worker design

#### Why introduce a generic `ThreadSafeQueue<T>` if Qt already has queued connections?

The generic queue makes producer/consumer semantics independently testable: close/drain behavior, local cancellation, MPMC integrity, ordering, and ownership can be verified without Qt or PJSUA2. It is a reusable concurrency primitive, not evidence that the production Qt path was rewritten to use it.

#### What is the difference between a consumer stop request and `close()`?

A stop token cancels one waiting consumer. It does not reject producers or close shared queue state. `close()` is queue-wide: future pushes are rejected and waiting consumers wake so already accepted work can be drained.

#### What ordering does the queue guarantee with multiple producers?

Successful pushes are serialized by the queue mutex. FIFO follows that linearization order, not wall-clock call order across independent producer threads.

#### Why does `SerialTaskWorker` drain instead of discarding queued tasks?

Accepted tasks are treated as work that must be accounted for before shutdown completes. Closing the queue and joining after drain provides a deterministic contract.

#### Why can a worker callback request close but not call `shutdown()`?

`shutdown()` joins the owned worker thread. Calling it from that same worker could self-join. `request_close()` stops new work without attempting the join from inside the callback.

### Verification and sanitizer scope

#### What does the 100,000-event stress test prove?

For the observed runs it verifies that multiple producers and consumers transfer 100,000 distinct IDs without missing, duplicate, malformed, or unexpectedly rejected items and that close/drain accounting holds. It does not prove every scheduler interleaving or live Qt/PJSUA2 callback behavior.

#### Why is a passing TSan run not proof that the softphone is race-free?

The recorded Clang 18 TSan gate instruments the standalone Phase 8 C++ tests, not every Qt/PJSUA2 runtime path. It is strong evidence for exercised code only.

#### Why was GCC 13 TSan recorded as BLOCKED instead of PASS?

The recorded GCC 13 runtime could not execute reliably because of sanitizer memory-mapping failures, including a minimal reproduction. An unusable sanitizer environment cannot be treated as race-free evidence. Clang 18 provided the successful standalone TSan run.

### Qt and PJSUA2 ownership

#### How does the production GUI path return work to the UI owner thread?

`QtRegistrationBackend` owns a session `std::jthread` and publishes registration, call, and RTP events using queued Qt invocation. Those callbacks execute on the QObject/UI side before mutating `GuiViewModel` or `CallManager`. The generic Phase 8 worker does not replace this path.

#### Why is `pjsua_call_get_count() == 0` not sufficient shutdown evidence?

It reports observed PJSUA call-slot state. It does not prove that every external or in-flight vendor callback referencing account/call-owned objects has completed. Call-slot cleanup and callback quiescence are therefore separate properties.

#### What is the purpose of the shutdown authorization helper?

It makes the policy decision explicit: teardown/integration is authorized only when modeled preconditions establish a safe-enough state. Unresolved shutdown failures, stale results, external callback activity, or uncertain ownership fail closed instead of being assumed safe.

#### Why can the roadmap exit audit PASS while integration readiness is BLOCKED?

They answer different questions. The roadmap audit checks that required source, tests, documentation, and evidence exist and are internally consistent. Integration readiness asks whether production PJSUA2 shutdown has enough evidence for a stronger safety claim. Phase 8 intentionally preserves `BLOCKED/UNPROVEN` for the latter.

### Verification boundary

#### What is the strongest defensible Phase 8 conclusion?

The project has a tested C++20 concurrency foundation, repeated MPMC stress, standalone TSan coverage, explicit Qt owner-thread handoff, and fail-closed PJSUA2 lifetime/shutdown models. It is not defensible to claim universal live PJSUA2 teardown safety, external callback quiescence, or that the generic queue is the deployed Qt/PJSUA2 event transport.

## Phase boundary

Phase 8 remains pending final maintainer acceptance. Live external callback quiescence and production-client real `PJ_EBUSY` recovery are **UNPROVEN**, and PJSUA2 integration readiness is **BLOCKED**.

Phase 9 must not be treated as started until Phase 8 is explicitly accepted.

## References

- `docs/phase8/README.md`
- `docs/architecture/threading-model.md`
- `docs/phase8/roadmap-exit-review.md`
- `docs/phase8/final-runtime-acceptance.md`
- `evidence/phase8/README.md`
- `evidence/phase8/roadmap-closure.md`
- `tests/unit/`
- `tests/integration/`
- `cmake/phase8-focused/`
- `scripts/phase8/`
