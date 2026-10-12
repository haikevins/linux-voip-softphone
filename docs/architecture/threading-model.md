# Phase 8 Architecture — Threading Model

## Status

**PENDING** — Phase 8 is merged to `main`, and its concurrency foundation and offline ownership/shutdown contracts are implemented and tested. Final maintainer acceptance has not been recorded.

Controlled live PJSUA2 `PJ_EBUSY` recovery and one lab-only early-shutdown / recovery cycle on the production-owned outgoing Account were observed. Other production-client recovery paths and external callback quiescence remain **UNPROVEN**; PJSUA2 integration readiness remains **BLOCKED**.

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

That earlier vendor-level probe alone did not prove production-client recovery or universal callback quiescence. The later P8-R2B experiment adds separate, bounded outgoing production-owned Account evidence.

### P8-R3C: isolated real-vendor dispatch observability

The P8-R3C lab is compiled **only** with `ENABLE_PHASE8_R3C_LAB=ON`
and a separately exported/build PJPROJECT 2.17 checkout. A scoped vendor
patch instruments the real `Endpoint::on_call_state` and
`Endpoint::on_call_media_state` paths **after** successful `Call::lookup()`
but **before** further C++ dispatch. An opt-in outgoing production client
records a paired, same-call-id/same-thread notification just before entering
its local `CallbackActivityGate`, and a bounded 500 ms vendor pause.
No `Call*` is passed to the hook, and the lab deliberately does not destroy
a `TrackedCall` while the hook is paused.

This observes two real vendor paths when the lab actually runs. It does not
instrument every C++ dispatch site (transaction, SDP, transfer, media jobs,
incoming call, failed `make_call()`), nor establish a barrier against any
callback source. Normal production builds do not compile the hook; the
normal installed PJPROJECT prefix is never modified. The source review's
four member release paths and temporary rejected Call paths remain audited
by P8-R3B. `UNPROVEN/BLOCKED` cannot be upgraded using a bounded event
observation or exit code alone.

## Verification strategy

All normal C++/Python contract tests are registered through the repository's canonical `tests/CMakeLists.txt`; there is no phase-specific CMake project.

Concurrency stress uses four producers and four consumers with 100,000 distinct IDs plus FIFO and close/drain scenarios. Passing runs establish the exercised invariants, not every possible scheduler interleaving.

ThreadSanitizer uses the root CMake project with `ENABLE_TSAN=ON` in a separate build. The recorded Clang 18 gate passed six selected C++ tests three times each (18/18). Vendor PJSUA2 and the complete Qt runtime were not thereby proven race-free.

Source-contract tests inspect production PJSUA2 shutdown structure and intentionally remain fail-closed: an offline audit may PASS while integration readiness remains BLOCKED.
The shutdown-attempt injection test additionally exercises the helper invoked by all three clients, without attributing fake errors to a live SIP session.

The opt-in P8-R2A lab (`labs/pjsip/production-stop-reuse/` and
`scripts/phase8/run-production-stop-reuse-lab.sh`) exercises one **production**
`SipOutgoingCallClient`: registration, an outgoing attempt, disconnection,
explicit stop, and registration after same-object restart. Its second `start()`
rejects retained teardown state. This tests the observed stop/reuse lifecycle,
not an early `shutdown2(false)` busy case. By itself it cannot prove real
production `PJ_EBUSY` recovery or external callback quiescence.

The opt-in P8-R2B lab differs from the P8-R2A normal-stop
observation: it invokes the **existing production Account** non-forced
`shutdown2(false)` through `shutdown_account_safely()` while a live
`TrackedCall` and vendor call slot are still present. A build-only
`VOIP_PHASE8_R2B_LAB` entry point classifies the actual `pj::Error::status`
and checks Account/Endpoint retention on `PJ_EBUSY`. It then performs normal
hangup and production `stop()`, and tests same-object restart to check that
shutdown state did not remain retained. The lab neither changes `stop()`
ordering nor bypasses callback activity gates in regular builds. A single
operator-approved run on 2026-10-12 observed `PJ_EBUSY`, retained owners,
cleanup and normal `stop()` returning, successful same-object restart, second
registration, final `stop()` and clean destructor/process exit. This is
**OBSERVED/EVIDENCED only for that outgoing-client lab sequence**; it does not
prove universal callback-source quiescence or every shutdown failure path.
See `evidence/phase8/p8-r2b-live-pj-ebusy-20261012.txt`.

### P8-R3A: callback-target ingress contract (offline only)

The three production clients declare nine known PJSUA2 callback overrides:
registration uses `onRegState`; outgoing and incoming each use `onRegState`,
`onIncomingCall`, `onCallState`, and `onCallMediaState`. Every override must
acquire its `CallbackActivityGate::enter()` RAII entry before loading the
`CallbackContext` owner, reject work when the gate is closed, and retain a
`shared_ptr<CallbackContext>` in the derived callback target. An added or
unguarded override now fails a dedicated source-contract regression test.

A concurrent rejected-entry regression verifies that `enter()` still counts
an already-running callback after the local gate closes, and that a later
callback can enter even after a previous local drain. This is evidence of the
**local** gate contract only. It does not show whether a PJSUA2 worker can
enter the virtual callback method after the derived callback target is
released. The shutdown-order review still relies on vendor lifecycle
behavior around `shutdown2(false)` and `libDestroy()`; those source/worker
quiescence guarantees require separate review and live observation.

### P8-R3B: vendor dispatch gap and controlled lifetime model

Reviewed PJPROJECT 2.17 (tag `2.17`, commit `5a457451fa2712ba18e12b01738e8ff3af2b26fd`)
associates C++ `Call*` through PJSUA-LIB `user_data`. The observed
`Call::lookup()` / `processMediaUpdate()` sequence can run **before** a derived
`TrackedCall` method enters the local `CallbackActivityGate`. The reviewed
DISCONNECTED call-count ordering is path-specific, not an all-dispatch join.

The P8-R3B controlled offline model uses a non-owning `weak_ptr` token to
represent a vendor lookup. A latch pauses the simulated callback between
lookup and gate entry. The test demonstrates that closing/draining the local
gate alone can permit **simulated** retirement while a pre-gate dispatch exists,
without dereferencing a retired object. A separate synthetic per-call ingress
fence preserves a token from lookup through callback return, tests release
veto until drain, and allows subsequent calls to use a separate fence.

`tests/integration/test_phase8_r3b_call_release_inventory.py` fails closed if
one of four known `call_.reset()` locations or four stack-local rejected
`pj::Call` creations is added, removed or relocated without review. The audit
covers outgoing `stop()`, `make_call()` exception cleanup and
`release_disconnected_call()`, incoming `stop()`, plus rejection branches in
both clients. It does **not** make source matching equivalent to correctness.

No synthetic fence has been attached to vendor `Call::lookup()` or production
teardown. The model neither reproduces a real use-after-free nor establishes
external callback quiescence. A real PJPROJECT worker dispatch proof and
instrumented lab, with no unsafe dereference, remain separate prerequisites
for claiming the stronger property.

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
- one controlled live vendor-level `PJ_EBUSY` recovery observation;
- one bounded P8-R2B real busy/recovery observation on a production-owned outgoing Account.

It does **not** establish live external callback quiescence, busy recovery for all production-client paths, universal race freedom, or production PJSUA2 integration readiness.

## References

- `docs/phase8/README.md`
- `docs/interview/phase8-questions.md`
- `evidence/phase8/README.md`
- `tests/unit/`
- `tests/integration/`
- `scripts/phase8/`
