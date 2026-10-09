# Phase 8 — Source review of Qt/PJSIP ownership (after Commit 7)

**Reviewed source:** the maintainer-provided `linux-voip-softphone.zip`, whose
`.git/refs/heads/phase/8-multithreading` resolves to `cf31e2ac6c19d64ef9e4546c64b11fff80ea3536`.
This is a **static review**, not a live SIP callback trace, and not a claim of
complete Qt/PJSIP thread safety. Locations below refer to that Git snapshot.

## Verified code paths

1. **GUI / state thread.** `src/app/qt_gui_main.cpp:83-101,110-134` creates
   `QGuiApplication`, backend, and view model in that order, and connects backend
   signals with the view model as context. The QML engine is constructed after
   them (`:142-161`), so its destruction precedes view model destruction, which
   precedes backend destruction. `src/application/CallManager.cpp:23-29` captures
   the constructing thread and asserts affinity; `src/application/CallManager.hpp:53-55`
   documents the borrowed-backend lifetime contract.
2. **Worker to GUI.** `src/gui/QtRegistrationBackend.cpp:36-53` starts a
   `std::jthread` only from the QObject owner thread. `:144-181,203-231,301-307,
   387-393,458-469` posts worker results with `Qt::QueuedConnection`, using
   the backend QObject as receiver. `:22-29` requests stop, notifies, and joins
   the worker before the QObject base destructor runs. These guards *support*
   safe handoff but need explicit teardown regression.
3. **Session ownership.** `src/gui/QtRegistrationBackend.cpp:401-473` constructs,
   uses, stops and destroys a `RegistrationSession` on the same worker. The
   production adapter (`src/gui/PjsipRegistrationSession.cpp:21-40`) delegates
   to `SipOutgoingCallClient`. `src/concurrency/SerialTaskWorker.hpp` has no
   application-side call sites in the reviewed source; it is not wired to SIP.
4. **PJSIP callbacks do work on callback invocation paths.** In
   `src/pjsip/SipOutgoingCallClient.cpp:19-45,62-90`, PJSUA2 callback overrides
   call the client directly. `:507-535` creates/answers incoming calls from
   `onIncomingCall`, while `:604-615` reads media and attaches audio from
   `onCallMediaState`. Therefore the source comments saying *all* PJSUA2
   operations occur on the `QtRegistrationBackend` worker overstate what the
   code establishes. Which OS threads execute these callbacks remains to be
   measured with a real PJSIP runtime trace.

## Risks and limits — not yet proven defects

- **R1 (high-priority shutdown order):** `SipOutgoingCallClient::stop()` at
  `src/pjsip/SipOutgoingCallClient.cpp:175-205` resets the `pj::Call` owner
  under callback locks at `:182-187`, *then* waits for
  `pjsua_call_get_count()==0` at `:189`. The independent incoming client
  follows the same order (`src/pjsip/SipIncomingCallClient.cpp:167-193`).
  This ordering deserves review against PJSUA2 call lifetime contracts and
  an in-flight callback test. The code alone does not prove UAF or deadlock.
- **R2 (account callback teardown):** account callbacks retain a reference
  to their owning client (`SipOutgoingCallClient.cpp:14-49`), but
  `onRegState` does not use the two call callback guards. Confirm
  `shutdown2` / `Account` destruction / `libDestroy` quiesce all registration
  and media callbacks before the owner is destroyed.
- **R3 (GUI responsiveness):** `QtRegistrationBackend` joins synchronously
  in its destructor and on failed-registration retry (`:22-29,36-53`). The
  mock Session API has no stop-aware wait (`QtRegistrationBackend.hpp:51-94`).
  Its production registration wait has an eight-second timeout (`:401-410`),
  so blocking on GUI teardown or retry is possible; measure rather than
  assume it is harmless.
- **R4 (enforcement):** `CallManager::check_thread()` uses `assert`
  (`CallManager.cpp:27-30`), disabled in NDEBUG builds. The integration
  currently relies on Qt routing and disciplined callers. Do not claim a
  runtime release-build off-thread guard for `CallManager`.
- **R5 (test boundary):** the Clang 18 Phase 8 TSan tests instrument only
  standalone queue/worker translation units. They do *not* cover PJSUA2,
  Qt queued lambdas, audio callbacks or endpoint shutdown.

## Regression added in this overlay

`tests/qt/test_qt_registration_backend.cpp` adds:

- `RejectsAllRequestEntryPointsFromForeignThread`: each public request method
  that must be called from the Qt owner thread is rejected on an external
  `std::thread`; the session factory is never invoked.
- `DestructionJoinsInFlightSessionWithoutDeliveringQueuedResult`: an injected
  registration session waits behind a bounded gate while the owner is
  destroyed; its release permits worker stop/join, then pumping Qt events
  must not deliver a stale completion signal. Fake state uses atomics/locks.

These are **fake-session Qt regression tests**, not real SIP stress tests. They
reuse the existing `QtRegistrationBackendTest` CTest entry, so the expected
canonical count remains **139**. Build/test on the maintainer's ThinkPad before
merging this overlay. No production code is changed in this slice.

## Manual runtime gate before claiming app-level closure

1. With a test Asterisk account, register and shut the GUI during pending
   registration, incoming ringing, active media, and remote disconnect.
2. Confirm PJSUA2 callback OS thread IDs, active `pj::Call` lifetime and
   shutdown ordering, and whether callbacks arrive after `shutdown2()`.
3. Verify no SIP call slot remains active before object destruction and that
   no callback touches account/endpoint memory after libDestroy.
4. Run canonical regression, Qt-focused regression and Phase 8 TSan again.
5. Record manual observations separately, without credentials/SIP URIs.

**Verdict: PARTIAL SOURCE AUDIT.** Qt handoff and fake-session lifetimes have
clear implementation boundaries. Real PJSIP callback quiescence and teardown
safety remain **UNVERIFIED**. Do not change SIP teardown ordering without a
PJSIP-supported integration test and an explicit failure-handling policy.
