# Phase 8 Interview Review — Multithreading

## Review scope

Base each answer on the implementation, tests, and retained evidence. Distinguish design intent, observed behavior, and unverified boundaries.

- `ThreadSafeQueue<T>` semantics and ownership
- `SerialTaskWorker` lifecycle
- repeated MPMC stress verification
- selected ThreadSanitizer scope
- Qt owner-thread handoff
- PJSUA2 callback/lifetime boundaries
- fail-closed shutdown authorization
- verified and unresolved Phase 8 claims

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

The recorded Clang 18 TSan gate instruments six selected C++ tests, not every Qt/PJSUA2 runtime path. It is strong evidence for exercised code only.

#### Why was GCC 13 TSan recorded as BLOCKED instead of PASS?

The recorded GCC 13 runtime could not execute reliably because of sanitizer memory-mapping failures, including a minimal reproduction. An unusable sanitizer environment cannot be treated as race-free evidence. Clang 18 provided the successful standalone TSan run.

### Qt and PJSUA2 ownership

#### How does the production GUI path return work to the UI owner thread?

`QtRegistrationBackend` owns a session `std::jthread` and publishes registration, call, and RTP events using queued Qt invocation. Those callbacks execute on the QObject/UI side before mutating `GuiViewModel` or `CallManager`. The generic Phase 8 worker does not replace this path.

#### Why is `pjsua_call_get_count() == 0` not sufficient shutdown evidence?

It reports observed PJSUA call-slot state. It does not prove that every external or in-flight vendor callback referencing account/call-owned objects has completed. Call-slot cleanup and callback quiescence are therefore separate properties.

#### What is the purpose of the shutdown authorization helper?

It makes the policy decision explicit: teardown/integration is authorized only when modeled preconditions establish a safe-enough state. Unresolved shutdown failures, stale results, external callback activity, or uncertain ownership fail closed instead of being assumed safe.

#### Why can offline verification PASS while integration readiness is BLOCKED?

They answer different questions. Offline tests verify the queue/worker and reviewed source contracts. Integration readiness asks whether production PJSUA2 shutdown has enough live evidence for a stronger safety claim. Phase 8 intentionally preserves `BLOCKED/UNPROVEN` for the latter.

### Verification boundary

#### What is the strongest defensible Phase 8 conclusion?

The project has a tested C++20 concurrency foundation, repeated MPMC stress, standalone TSan coverage, explicit Qt owner-thread handoff, and fail-closed PJSUA2 lifetime/shutdown models. It is not defensible to claim universal live PJSUA2 teardown safety, external callback quiescence, or that the generic queue is the deployed Qt/PJSUA2 event transport.

### Production shutdown retry verification

#### Does an injected `PJ_EBUSY`-like failure prove that a production client can recover?

No. The injection passes through the shared account-shutdown invocation helper
used by the production clients and verifies error-to-result handling and a
modeled ownership/retry decision. It does not execute the real PJSUA2
`SipOutgoingCallClient::stop()` or `SipIncomingCallClient::stop()` path against
an active call. A production live observation and a defensible external callback
lifetime proof are still separate requirements.

#### What does an observed production-client stop/reuse cycle establish?

With a real Asterisk registration and outgoing call attempt, P8-R2A can show
that one `SipOutgoingCallClient` stopped, restarted on the **same object**, and
registered again without failing its retained-resource startup guard. It does
not deliberately reach an active-call `shutdown2(false)` `PJ_EBUSY` condition;
that remains the separate P8-R2B objective. A completed stop/reuse observation
also cannot prove quiescence for all possible external callbacks.

#### How can the real production-owned account `PJ_EBUSY` case be observed?

An opt-in P8-R2B build can attempt the non-forced account shutdown on the
actual `SipOutgoingCallClient` Account immediately after it starts an outgoing
call, before the ordinary `stop()` call-slot cleanup. The returned PJSUA2
`pj::Error::status` must be precisely `PJ_EBUSY`; a generic caught exception,
a mock or a separate `ProbeAccount` is insufficient. The lab checks that its
Account and Endpoint remain owned, hangs up, runs the normal stop path, and
requires a successful same-object restart and second registration. This is a
bounded experiment, not a change to the normal production teardown order.

#### What did the P8-R2B live run actually establish?

The 2026-10-12 lab run observed a real PJSUA2 `PJ_EBUSY` from non-forced
shutdown on the production-owned outgoing Account, retained owners immediately
after the error, then disconnect, normal production `stop()` returning and
restart of the same object with a second successful registration. The lab
reported process exit `0`. It did not independently record the vendor status
of the later `stop()` shutdown call; a returned stop and successful restart
support recovery in this tested sequence. Other clients and ordinary-stop
busy paths were not exercised.

#### Why would a P8-R2B live PASS not close external callback quiescence?

Seeing a real busy error and successful recovery tests one controlled
production-client sequence only. It does not establish that every possible
late external PJSUA2 callback has ceased before object destruction. That
claim remains **UNPROVEN** independently of the lab outcome.

### Callback ingress and external source quiescence

#### Does a drained `CallbackActivityGate` prove PJSUA2 has stopped its callbacks?

No. `enter()` tracks a callback only **after** PJSUA2 has entered the C++
virtual method. After `close()`, a callback already inside `enter()` is
counted through method return, even when owner work is rejected. A later
external callback can still attempt entry after a zero-count observation.
Therefore, the target must remain alive until the PJSUA2 callback source
itself can no longer invoke the callback. The P8-R3A source and thread tests
check local ingress invariants, not that external vendor guarantee.

#### Why must newly added `on*` callback overrides trigger explicit review?

An added PJSUA2 virtual override could bypass the activity gate and load an
owner during teardown. The P8-R3A contract audits the complete currently
known override set across registration, outgoing, and incoming clients,
including their shared callback-context ownership and fail-closed destructor
order. A changed set fails the audit until the new entry path is reviewed.

### P8-R3B: vendor dispatch lifetime modeling

#### What is the dangerous interval that P8-R3B models?

PJPROJECT can fetch a non-owning C++ `Call*` through `Call::lookup()` and run
base C++ processing before entering the derived callback override. The local
activity gate counts only entries that reach the override. A pre-gate lookup
can therefore exist when the local gate reports empty. The deterministic test
pauses a **synthetic** callback in that interval and checks expiration through
a `weak_ptr` without dereferencing a retired target.

#### Why is a synthetic per-call external fence useful but insufficient?

It demonstrates the stronger contract that would be needed: new dispatch
admission is stopped, and all dispatch tickets held **from lookup to return**
are drained before retirement. Separate fences permit later calls without
reopening an old target. PJPROJECT's actual lookup path has not been shown to
acquire such a ticket, so model PASS is not vendor quiescence PASS.

#### Which Call destruction paths require explicit review?

Four existing member resets (three outgoing: stop, `make_call()` exception,
`release_disconnected_call()`; one incoming: stop) and four temporary rejected
`pj::Call` instances (two in each call client). A canonical source inventory
test fails if this set drifts. The failure/temporary paths need dedicated
vendor lifetime analysis before a production code change.

## Phase boundary

Phase 8 remains pending final maintainer acceptance. One production-owned
outgoing Account lab-only real `PJ_EBUSY` recovery sequence was observed.
Recovery for other production-client paths and live external callback
quiescence remain **UNPROVEN**, and PJSUA2 integration readiness is **BLOCKED**.

Phase 9 must not be treated as started until Phase 8 is explicitly accepted.

## References

- `docs/phase8/README.md`
- `docs/architecture/threading-model.md`
- `evidence/phase8/README.md`
- `tests/unit/`
- `tests/integration/`
- `scripts/phase8/`
