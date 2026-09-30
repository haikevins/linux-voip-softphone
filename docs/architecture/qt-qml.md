# Phase 7 Architecture — Qt/QML UI boundary

## Status

Commit 1 introduced an optional Qt6/QML dialer shell. Commit 2 added a Qt-free `CallManager` with an injectable `CallBackend` and observable snapshots. Commit 3 adds an opt-in, production-backed **SIP registration adapter only**. It does not implement GUI calling, incoming-call answering, audio routing or RTP diagnostics.

## Purpose

Connect the Qt GUI to the existing Phase 6 registration implementation through an explicit application-layer boundary. Preserve the existing SIP/RTP CLI, keep PJSUA2 out of QML and deliver asynchronous registration results to the Qt UI thread without publishing sensitive backend information.

## Scope

Commit 3 covers opt-in initial SIP registration, a Qt-to-CallManager bridge, dedicated registration-worker ownership, a retained successful registration session and tests with injected fake sessions. It does not provide continuous registration-health monitoring, automatic retries, account switching or deregistration monitoring.

`ENABLE_QT_GUI=OFF` preserves CLI/headless operation. Enabling Qt alone builds and tests the bridge with fake registration sessions and no PJSIP linkage. The combined `qt-pjsip-gui-debug` preset builds the production registration adapter alongside the existing Phase 6 SIP implementation.

## Architecture model

```text
QML (status, Register button and local dial-pad UI)
  |
  v
GuiViewModel (QObject, UI-thread owner of CallManager)
  |
  v
CallManager (Qt-free core state machine, explicit async events)
  |
  v
CallBackend API
  |
  v
QtRegistrationBackend (QObject, UI thread + dedicated worker)
  |
  v
RegistrationSession interface
  |
  v
PjsipRegistrationSession (Qt+PJSIP builds only)
  |
  v
Existing Phase 6 SipRegistrationClient / PJSUA2
```

`CallManager` reuses `CallStateMachine`. An accepted `begin_registration()` request enters `Registering` but does not imply success. An explicit queued backend event is the only route to `Ready` or `Failed`. `CallBackend::supports_calls()` is a capability flag: the registration-only backend returns `false`, so calls remain disabled even after successful registration. An absent backend disables both registration and calling.

## Runtime and lifecycle

`GuiViewModel` owns `CallManager` on the Qt UI thread. `QtRegistrationBackend` is a QObject on that same thread. Its `begin_registration()` starts a dedicated worker rather than executing the SIP operation synchronously. The worker constructs, starts, waits on and destroys its `RegistrationSession`. A successfully registered session remains alive until application shutdown rather than being destroyed immediately after success; a failed or timed-out session is stopped before the result is delivered.

The worker reports initial registration through `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`. `registrationFinished(bool)` is emitted on the QObject's owning thread and connected to `CallManager::on_registration_succeeded/failed` through the ViewModel. The worker never calls `CallManager` directly. In `qt_gui_main.cpp`, the QML engine is destroyed before the ViewModel, and the backend is destroyed last; the backend requests worker stop and joins it before destroying the QObject context.

Shutdown may wait for the eight-second registration wait, plus PJSIP startup and cleanup. PJSUA2 lifecycle and cleanup stay on the thread that created `SipRegistrationClient`. Continuous account-state monitoring and multi-session call ownership are not implemented here.

## Verification strategy

`QtRegistrationBackendTest` injects fake sessions to test queued UI-thread result delivery, worker ownership, persistent successful registration, failure and retry without live SIP traffic. `CallManagerTest` exercises registration-only capabilities and asynchronous state transitions. `QtGuiShellSmokeTest` runs without a SIP account or network operations and verifies that registration and calling are disabled in inert mode.

Build and test the Qt-only and combined Qt/PJSIP configurations independently, then run inherited Phase 6 automated verification. Separately, use controlled Asterisk registration to confirm the production adapter. Passing automated tests alone is not evidence of a successful live REGISTER or of continuous registration health.

## Design rationale

The default GUI has no backend and reads no credentials. Production registration is explicitly enabled through `--sip-register USER SERVER [PORT]`; the password is read from stdin before the GUI starts, never from QML or command-line arguments. The backend reports a boolean instead of publishing raw SIP exceptions, credentials, Digest headers or PJSUA2 objects. The GUI uses generic error text because third-party exception messages could include private data.

Separating `RegistrationSession` from the Qt bridge allows deterministic, network-free tests. Retaining a successful session also avoids claiming a registration that the application has already torn down.

## Phase boundary

Commit 3 ends at initial SIP registration. Its registration-only backend deliberately rejects outgoing-call requests and keeps the QML Call button disabled even in `Ready`. Actual calls, incoming-call handling, media routing, RTP diagnostics, continuous registration status and the broader concurrency model remain future work. The subsequent integration should reuse this account/session ownership boundary rather than creating another competing PJSIP endpoint in QML.

## References

- `README_roadmap.md`
- `docs/phase7/README.md`
- `docs/architecture/audio-rtp.md`
- `src/application/CallManager.hpp`
- `src/gui/QtRegistrationBackend.hpp`
- `src/gui/PjsipRegistrationSession.hpp`
- `tests/unit/test_call_manager.cpp`
- `tests/qt/test_qt_registration_backend.cpp`
