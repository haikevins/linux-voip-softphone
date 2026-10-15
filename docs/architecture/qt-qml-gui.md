# Phase 7 Architecture — Qt/QML GUI

## Status

Implementation and automated verification are complete on the Phase 7 branch. Live SIP/audio GUI acceptance and milestone closure are still pending. Target milestone: `v0.8-qt-gui`.

## Purpose

Add a Qt 6/QML presentation layer to the existing softphone without making QML responsible for SIP/PJSUA2 lifecycle, call-state transitions, or backend synchronization.

The Phase 7 architecture introduces three reusable boundaries:

```text
src/gui/CallViewModel.*          Qt/QML-facing presentation adapter
src/application/CallManager.*   application/domain controller
src/application/SipEngine.hpp   backend interface
```

and one production backend:

```text
src/pjsip/Pjsua2SipEngine.*
```

This converts the Phase 5/6 one-shot CLI integration into a backend that can remain alive for an interactive GUI session while still reusing the previously verified PJSUA2 audio path.

## Scope

This architecture covers:

- QML-to-C++ call control through a QObject ViewModel;
- registration settings and backend construction;
- application-level call-state ownership;
- outgoing/incoming call direction and actions;
- long-lived endpoint/account/call ownership in `Pjsua2SipEngine`;
- callback-to-controller state translation;
- queued Qt-facing refresh after asynchronous backend callbacks;
- Qt-only and Qt+PJSIP build boundaries;
- deterministic fake-engine tests and headless GUI launch verification.

Outside this architecture document's Phase 7 boundary are the Phase 8 worker-thread model, multi-call/conference policy, persistent credential storage, production notification/background-service behavior, and new UI controls for mute/audio-device selection/RTP statistics.

## Architecture model

### Layering and dependency direction

```text
+---------------------------+
|          Main.qml         |
| fields / buttons / views  |
+-------------+-------------+
              |
              | Q_PROPERTY / Q_INVOKABLE
              v
+---------------------------+
|       CallViewModel       |
| Qt types + presentation   |
+-------------+-------------+
              |
              | application actions/state
              v
+---------------------------+
|        CallManager        |
| CallStateMachine + policy |
+-------------+-------------+
              |
              | SipEngine interface
              v
+---------------------------+
|       Pjsua2SipEngine     |
| endpoint/account/call     |
+-------------+-------------+
              |
              v
        PJSUA2 / SIP / RTP
              |
              v
       Asterisk / peer
```

Dependencies point downward. `Main.qml` does not include PJSIP knowledge, `CallViewModel` does not construct PJSUA2 classes directly, and `CallManager` depends on `SipEngine` rather than the PJSUA2 implementation.

`src/gui/main.cpp` is the composition root. It converts `SipConnectionSettings` into `SipEndpointConfig` and `SipAccountConfig`, constructs `Pjsua2SipEngine` when `ENABLE_PJSIP` is active, constructs `CallViewModel` with that factory, and exposes the ViewModel to QML as `callViewModel`.

### `CallViewModel` responsibility

`CallViewModel` translates between Qt-facing data and application-domain operations.

It exposes these main read-only properties:

```text
stateText
peer
errorText
accountText
callDurationText
incomingCall
canRegister
canCall
canAnswer
canHangup
```

and these QML-callable actions:

```text
registerAccount(...)
makeCall(destination)
answerCall()
hangupCall()
resetSession()
```

The ViewModel validates presentation-level input such as empty fields, valid port ranges, and UDP/TCP selection. It owns the current engine/controller session and converts domain state into UI enable/visibility decisions.

It does not decide SIP response codes, create `pj::Endpoint`/`pj::Account` objects, or manipulate RTP/audio media directly.

### `CallManager` responsibility

`CallManager` is the application controller and `SipEngineObserver`.

For UI actions it:

```text
register_account() -> RegisterRequested -> engine.register_account()
make_call()        -> OutgoingCallRequested -> engine.make_call()
answer_call()      -> validate Ringing -> engine.answer_call()
hangup_call()      -> HangupRequested -> engine.hangup_call()
reset()            -> ResetRequested
```

For backend callbacks it maps:

```text
registration succeeded -> RegistrationSucceeded
registration failed    -> RegistrationFailed
incoming call          -> IncomingCallReceived
remote ringing         -> RemoteRinging
call connected         -> CallConnected
call ended             -> CallEnded
call failed            -> CallFailed
```

The controller owns the existing `CallStateMachine`, protects it with `state_mutex_`, and exposes copies of the current state/peer/error plus the incoming-call direction flag.

### `SipEngine` boundary

`SipEngine` intentionally has a small call-control surface:

```text
set_observer(...)
register_account()
make_call(destination)
answer_call()
hangup_call()
```

The associated observer reports semantic events rather than PJSUA2 types. This keeps application and GUI code independent from PJSUA2 headers and makes tests able to inject a fake engine.

### Production and Qt-only backends

With `ENABLE_PJSIP=ON`, the factory in `main.cpp` creates `Pjsua2SipEngine`.

With Qt enabled but PJSIP disabled, the GUI still builds with an internal unavailable backend. The shell can launch and the UI/tests can exercise Qt integration, but backend operations fail with a clear message explaining that `qt-pjsip-debug` is required for real SIP work.

This protects the optional-dependency boundary established in earlier phases.

## Runtime and lifecycle

### Application startup

```text
QGuiApplication
      |
      v
create backend factory
      |
      v
construct CallViewModel
      |
      v
expose callViewModel to QML context
      |
      v
load Main.qml
      |
      v
Qt event loop
```

No PJSUA2 endpoint is created merely by opening the window. A backend session is created when the user submits registration settings.

### Registration lifecycle

```text
QML Register
    |
    v
CallViewModel::registerAccount()
    | validate/normalize UI fields
    | rebuild engine + CallManager session
    v
CallManager::register_account()
    | RegisterRequested
    v
Registering
    |
    v
Pjsua2SipEngine::register_account()
    |
    v
PJSUA2 registration callback
    |
    +--> success -> CallManager -> Ready
    |
    +--> failure -> CallManager -> Failed
```

The password is copied into backend configuration in process memory but is not exposed as a ViewModel status/diagnostic property. The QML password editor uses masked input.

### Outgoing call lifecycle

```text
Ready
  |
  | QML Call
  v
CallViewModel::makeCall()
  |
  v
CallManager::make_call()
  |
  +--> state: Calling
  |
  v
Pjsua2SipEngine::make_call()
  |
  +--> remote ringing callback -> Ringing
  |
  +--> confirmed callback      -> Connected
  |
  +--> failure                 -> Failed
  |
  +--> local/remote hangup     -> Terminating -> Ready
```

`peer` stores the dialed destination. `incoming_call_` is false for this path.

### Incoming call lifecycle

```text
Ready
  |
  v
PJSUA2 account onIncomingCall()
  |
  v
Pjsua2SipEngine creates/tracks call
  |
  v
SipEngineObserver::on_incoming_call(peer)
  |
  v
CallManager -> Ringing + incoming_call_=true
  |
  +--> QML Answer -> engine.answer_call() -> Connected
  |
  +--> QML Hangup -> Terminating -> Ready
```

Incoming and outgoing calls share the `Ringing` state. The separate `incoming_call_` flag tells the ViewModel whether Answer should be enabled.

### Callback and Qt event-loop handoff

Backend callbacks can occur asynchronously relative to the QML/UI event loop.

`CallManager` first handles the domain event under `state_mutex_`. `CallStateMachine` notifies its `StateChangeSink`, which is the `CallViewModel`. The ViewModel deliberately does **not** immediately call back into `CallManager` from `on_state_changed()` because the manager may still hold its mutex.

Instead it schedules:

```text
QMetaObject::invokeMethod(
    this,
    refresh_from_manager,
    Qt::QueuedConnection)
```

Conceptually:

```text
PJSUA2 callback context
       |
       v
CallManager mutex + state transition
       |
       v
ViewModel on_state_changed()
       |
       | queued Qt invocation
       v
return from callback / release controller lock
       |
       v
Qt event loop executes refresh_from_manager()
       |
       v
Q_PROPERTY state + signals update QML
```

This has two Phase 7 purposes: avoid re-entrant locking against `CallManager` and ensure the Qt-facing refresh is delivered through the ViewModel's event queue. It is not the complete Phase 8 worker-thread architecture.

### Session reset and destruction

`CallViewModel` owns the backend before the controller so it can explicitly reset the controller first, detach its observer, and then release the engine. `Pjsua2SipEngine` is responsible for stopping active PJSUA2 objects in its destructor/stop path.

A failed GUI session can be reset to `Idle`, after which a new backend/controller session is constructed from newly entered settings.

## Verification strategy

Phase 7 separates deterministic application/GUI tests from environment-dependent SIP/audio acceptance.

### Deterministic controller tests

`tests/unit/test_call_manager.cpp` injects a fake `SipEngine` and verifies registration, outgoing/incoming call state, answer/hangup behavior, exception-to-failure mapping, and rejection of invalid operations without contacting a network.

### Deterministic ViewModel tests

`tests/unit/test_call_view_model.cpp` injects an engine factory/fake backend and verifies input validation, settings mapping, outgoing/incoming UI action state, queued callback propagation, failure presentation, and session reset behavior.

### GUI launch smoke test

`scripts/phase7/verify-gui-smoke.sh` launches the built GUI with:

```text
QT_QPA_PLATFORM=offscreen
QT_QUICK_BACKEND=software
```

and requires it to remain alive in the Qt event loop for the three-second smoke window. This catches startup/QML-load failures without requiring a display server or live SIP account.

### Combined automated gate

`scripts/phase7/verify.sh` inherits Phase 6 quality/regression verification, builds Qt-only and Qt+PJSIP configurations, runs Phase 7-labeled tests, runs the GUI smoke test through CTest, and verifies documentation/evidence structure.

### Manual integration gate

The milestone still requires a human-operated GUI against the controlled Asterisk lab to verify registration, outgoing/incoming calls, visible state changes, and two-way intelligible audio. Automated tests intentionally do not manufacture evidence for those environment-dependent outcomes.

## Design rationale

### Why QML does not call PJSIP directly

Direct PJSIP calls from QML would mix presentation, library object lifetime, SIP error handling, and asynchronous callback behavior in one layer. It would also make deterministic testing difficult and make a future UI replacement unnecessarily expensive.

The ViewModel/controller/backend split gives each layer one primary reason to change.

### Why `CallManager` is separate from `CallViewModel`

Call-state rules are application logic, not Qt presentation logic. Keeping them in `CallManager` allows the same controller/state machine to be tested without a QML engine and prevents button visibility rules from becoming the authoritative call lifecycle.

### Why `SipEngine` is an interface

The interface prevents PJSUA2 types from leaking into the application layer and enables fake-engine unit tests. It also leaves room for alternative backends without rewriting the UI/controller contract.

### Why the backend is long-lived

The Phase 5/6 CLI clients perform one bounded operation and exit. A GUI must keep account and call objects alive across multiple user events and asynchronous callbacks. `Pjsua2SipEngine` therefore owns endpoint/account/call state for the active ViewModel session.

### Why queued refresh is preferred over direct refresh in the callback

A direct refresh could immediately call manager getters while `CallManager` still holds `state_mutex_`, creating a self-deadlock/re-entrancy hazard. Queueing the refresh lets the transition callback return first and places the Qt-facing update on the ViewModel event queue.

### Why the Qt-only backend exists

Qt/QML compilation and startup should be testable even on a machine where PJPROJECT is intentionally absent. The unavailable backend preserves that build boundary while producing an explicit runtime error if the user attempts SIP operations.

### Why manual acceptance remains necessary

A fake engine can prove state mapping and a smoke test can prove that the window loads, but neither proves successful SIP transactions, remote ringing, physical microphone/speaker routing, or intelligible speech. Those claims require the real PJSUA2 backend and the controlled lab.

## Phase boundary

Phase 7 establishes the presentation/application/backend architecture and a safe callback-to-Qt handoff for one active call.

It does not yet introduce the Phase 8 architecture of explicit SIP/metrics/logging worker threads, general thread-safe work queues, or broader concurrent task scheduling. Phase 8 should build on these boundaries rather than moving PJSIP responsibilities into QML.

The current QML diagnostics are intentionally simple state/account/peer/direction/duration observations. Rich RTP statistics, dedicated audio-device controls, microphone-level visualization, mute policy, multi-call behavior, and production credential persistence remain outside this Phase 7 architecture unless separately added and verified later.

## References

- `docs/phase7/README.md`
- `docs/architecture/call-state-machine.md`
- `docs/architecture/pjsip-cli.md`
- `docs/architecture/audio-rtp.md`
- `src/gui/CallViewModel.*`
- `src/gui/Main.qml`
- `src/application/CallManager.*`
- `src/application/SipEngine.hpp`
- `src/pjsip/Pjsua2SipEngine.*`
- `scripts/phase7/verify.sh`
- `scripts/phase7/verify-gui-smoke.sh`
