# Phase 7 Interview Review — Qt/QML GUI

Use the repository implementation, automated tests, live GUI evidence, and architecture documentation as the basis for these answers. Explain the dependency boundaries and failure modes rather than treating QML as only a visual layer.

## Review scope

- Qt/QML presentation architecture
- ViewModel/application/backend separation
- `CallManager` and state-machine ownership
- reusable `SipEngine` abstraction
- long-lived `Pjsua2SipEngine` lifecycle
- callback-to-Qt event-loop handoff
- Qt-only versus Qt+PJSIP builds
- controller/ViewModel testing and GUI smoke verification
- manual SIP/audio acceptance and Phase 8 concurrency boundary

## Questions and answers

### Layering and ownership

#### What is the main Phase 7 architecture?

The dependency chain is:

```text
QML -> CallViewModel -> CallManager -> SipEngine -> Pjsua2SipEngine
```

QML is responsible for presenting fields, buttons, status, and diagnostics. `CallViewModel` adapts Qt-facing values/actions. `CallManager` owns application call-state policy. `SipEngine` is the backend contract, and `Pjsua2SipEngine` owns the PJSUA2 endpoint/account/call implementation.

#### Why should QML not call PJSIP directly?

PJSIP/PJSUA2 has library lifecycle, object ownership, callbacks, errors, call/media state, and concurrency concerns. Putting those calls directly in QML would mix presentation code with backend policy and make the call lifecycle difficult to test independently.

The current boundary also allows a fake `SipEngine` to drive deterministic tests without a SIP server.

#### What is the difference between `CallViewModel` and `CallManager`?

`CallViewModel` contains Qt-facing concerns: `QString` conversion, Q_PROPERTY values, field validation, action availability, duration presentation, Qt signals, and queued refresh into the event loop.

`CallManager` contains application concerns: the `CallStateMachine`, valid action/state transitions, incoming/outgoing direction, backend calls, and translation of `SipEngineObserver` events into domain events.

A useful test is: if the QML UI were replaced, most call-state rules should remain unchanged in `CallManager`.

#### Why introduce the `SipEngine` interface instead of letting `CallManager` depend on `Pjsua2SipEngine`?

The interface prevents PJSUA2 types from leaking into the application layer. It also makes `CallManager` and `CallViewModel` testable with fake engines and keeps the architecture open to a different backend without rewriting the UI/controller contract.

#### Where is the concrete backend selected?

`src/gui/main.cpp` is the composition root. The ViewModel supplies `SipConnectionSettings` to a factory. In a Qt+PJSIP build the factory converts those values into PJSIP endpoint/account configuration and constructs `Pjsua2SipEngine`.

This keeps object composition outside QML and outside the application controller.

### State and call control

#### Which states are presented by the GUI controller?

The existing state machine uses:

```text
Idle
Registering
Ready
Calling
Ringing
Connected
Terminating
Failed
```

The ViewModel converts the current state into `stateText` and derives button/action availability from it.

#### How can both incoming and outgoing calls use the same `Ringing` state?

`CallManager` keeps a separate `incoming_call_` direction flag.

For an outgoing call, remote ringing transitions `Calling -> Ringing` with the flag false. For an incoming call, `IncomingCallReceived` transitions `Ready -> Ringing` with the flag true.

The ViewModel therefore enables Answer only when the state is `Ringing` **and** `incomingCall` is true.

#### Why does `CallManager` call the state machine before invoking some backend actions?

The state machine is the application's authority on whether an action is valid. For example, an outgoing call request first attempts the `Ready -> Calling` transition. If the transition is invalid, the backend is not called.

This prevents the GUI/backend from becoming a second inconsistent source of lifecycle rules.

#### What happens when a backend operation throws an exception?

`CallManager` catches standard and unknown exceptions around SIP operations and converts them into application failure events/reasons. The ViewModel reads the resulting error text and presents it to QML instead of allowing a backend exception to escape through the UI boundary.

### PJSUA2 lifecycle and callbacks

#### Why is `Pjsua2SipEngine` long-lived when the Phase 5 clients were one-shot?

The CLI model was intentionally:

```text
configure -> perform one operation -> teardown -> exit
```

A GUI receives user actions over time and must keep registration/account/call objects alive while callbacks can arrive asynchronously. `Pjsua2SipEngine` therefore owns the endpoint, account, and current call across the active GUI session.

#### How do PJSUA2 callbacks reach QML-visible state?

At a high level:

```text
PJSUA2 callback
    -> SipEngineObserver callback on CallManager
    -> CallStateMachine transition
    -> StateChangeSink callback on CallViewModel
    -> queued refresh_from_manager()
    -> Q_PROPERTY/Qt signals
    -> QML redraw/enablement
```

QML never receives PJSUA2 callback objects directly.

#### Why does `CallViewModel::on_state_changed()` use `Qt::QueuedConnection`?

The state-change sink can be invoked while `CallManager` still holds its state mutex. Calling manager getters immediately from the sink would try to re-enter the same protected state and risks deadlock.

The ViewModel instead queues `refresh_from_manager()`. The manager callback returns, the lock can be released, and the Qt event loop later refreshes the properties.

This also provides the correct event-loop handoff for Qt-facing updates, but it is not a substitute for the more explicit Phase 8 worker-thread architecture.

#### Why does `CallManager` protect state with a mutex?

UI actions and PJSUA2 callbacks do not have to originate from the same execution context. The controller exposes a single state machine and peer/error/direction data, so those reads and transitions need serialized access.

The mutex protects the application state; it does not mean all PJSUA2 work has been moved off the UI thread.

### Qt build and UI behavior

#### What is the difference between `qt-debug` and `qt-pjsip-debug`?

`qt-debug` enables Qt/QML but leaves PJSIP disabled. It verifies that the GUI can compile, link, load QML, and participate in automated tests without requiring PJPROJECT. SIP operations use an unavailable backend and return a clear runtime error.

`qt-pjsip-debug` enables both Qt/QML and PJSIP and constructs the real `Pjsua2SipEngine` backend.

#### Why keep a Qt-only build if the product ultimately needs PJSIP?

It preserves the repository's optional dependency boundary and makes Qt/UI failures distinguishable from PJPROJECT integration failures. CI or a developer can validate the QML shell and fake-engine tests even when the SIP dependency is unavailable.

#### What does the current GUI expose?

The current QML screen provides:

- account registration inputs;
- UDP/TCP selection;
- state/account status;
- a destination/dialer flow;
- incoming answer and hangup actions;
- connected-call duration;
- simple diagnostics for state, account, peer, direction, and duration;
- error presentation and session reset/retry behavior.

The Phase 7 implementation does not add dedicated mute, microphone-level, audio-device-selection, or RTP-statistics controls to the QML screen. Those should not be claimed in evidence unless they are separately implemented and verified.

### Testing and evidence

#### How are `CallManager` tests made deterministic?

The tests inject a fake `SipEngine`. They can record requested operations and explicitly trigger observer callbacks such as registration success, incoming call, ringing, connected, ended, and failure.

No SIP server, audio device, or network timing is required to verify the controller/state mapping.

#### What do the `CallViewModel` tests verify?

They verify UI-facing behavior without requiring the real QML window or live PJSIP backend: registration input validation, settings mapping, action enablement, outgoing/incoming state, backend failure presentation, queued callback propagation, and reset behavior.

#### What does the GUI launch smoke test prove?

It runs the GUI offscreen with a software Qt Quick backend and expects the process to remain in the Qt event loop for a fixed three-second window.

That proves the built executable can start and load its QML sufficiently to stay alive. It does not prove SIP registration, remote calling, screen appearance on every desktop stack, or audio quality.

#### Why does Phase 7 still require manual testing after all automated tests pass?

Automated fake-backend tests cannot prove that a real Asterisk registration succeeds, that another endpoint rings, that incoming/outgoing calls traverse the real PJSUA2 stack, or that microphone/speaker audio is intelligible.

The milestone therefore separates deterministic automated verification from live GUI/SIP/audio acceptance evidence.

#### What evidence should not be committed?

Do not commit SIP passwords, Digest authorization material, SRTP/media keys, raw authentication-bearing PJSIP logs, or packet captures. Screenshots must not expose the password field contents.

Evidence should record sanitized states/results and the tested Git revision.

### Phase boundary and trade-offs

#### Is Phase 7 already a fully multithreaded desktop architecture?

No. It has mutex-protected application state and a queued Qt refresh path because PJSUA2 callbacks are asynchronous, but the explicit worker-thread/queue design is intentionally deferred to Phase 8.

Phase 8 should make thread ownership and blocking-work placement explicit rather than adding uncontrolled threads inside the ViewModel or QML.

#### Why not implement every roadmap mockup control in the first GUI milestone?

The important architectural objective is to prove the presentation/controller/backend boundary and live call control on top of the already verified media path. Adding unrelated UI controls before this boundary is verified would increase surface area without improving the core design proof.

Missing controls must remain documented as limitations rather than being implied by screenshots or roadmap sketches.

#### What is the strongest architecture point to explain in an interview?

The GUI is not a thin file full of PJSIP calls. The project establishes an explicit dependency inversion:

```text
QML -> ViewModel -> application controller -> backend interface -> PJSUA2
```

That separation improves testability, object ownership, error handling, future threading work, and the ability to reason about SIP callbacks independently from the presentation layer.

## Phase boundary

Phase 7 proves the Qt/QML presentation boundary, application call controller, reusable SIP backend, deterministic GUI/controller tests, and GUI launch integration for one active call.

Live SIP/audio evidence must still close the milestone. Explicit worker queues/threads, richer runtime metrics, multi-call behavior, persistent credential storage, and other production desktop concerns remain outside this phase and are candidates for later milestones, especially Phase 8 concurrency work.

## References

- `docs/phase7/README.md`
- `docs/architecture/qt-qml-gui.md`
- `docs/architecture/call-state-machine.md`
- `docs/architecture/audio-rtp.md`
- `src/gui/CallViewModel.*`
- `src/gui/Main.qml`
- `src/application/CallManager.*`
- `src/application/SipEngine.hpp`
- `src/pjsip/Pjsua2SipEngine.*`
- `tests/unit/test_call_manager.cpp`
- `tests/unit/test_call_view_model.cpp`
- `scripts/phase7/verify.sh`
