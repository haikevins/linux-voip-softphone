# Phase 7 Architecture — Qt/QML UI boundary

## Status

In progress. Commit 1 introduced an optional Qt6 GUI shell. Commit 2 adds a Qt-free application facade and an observable ViewModel, but no production SIP adapter. Live SIP operations are not enabled in the GUI.

## Purpose

Establish a Qt/QML boundary that can display VoIP status and request call operations in later commits without exposing PJSUA2 objects or running SIP work from QML.

## Scope

This document covers the GUI target, its observable ViewModel, the new application-layer CallManager facade, and planned boundaries with the existing C++ core and PJSIP components. SIP/account lifecycle, incoming-call handling, audio routing, and diagnostics integration are future Phase 7 work.

## Architecture model

The roadmap's target dependency direction is:

```text
QML view
    |
    v
QObject ViewModel (UI-thread ownership)
    |
    v
CallManager (Qt-free application facade, Commit 2)
    |
    v
CallBackend interface -> SipEngine / PJSUA2 adapter (future integration)
```

Commit 2 adds `src/application/CallManager.*` to `voip::core`. It wraps the existing `CallStateMachine`, accepts a borrowed `CallBackend` interface, publishes a copyable `CallSnapshot`, and never creates a PJSUA2 endpoint. Registration, outgoing call and hangup requests represent *accepted commands*, not successful SIP operations. Only later backend lifecycle events can move an accepted registration to `Ready` or a call to `Connected`. The default GUI constructs its ViewModel with no backend, so the Call button stays disabled.

## Runtime and lifecycle

`ENABLE_QT_GUI=OFF` leaves the established CLI and Phase 6 targets unchanged. When enabled, CMake requires Qt6 Quick and QuickControls2 and builds `voip-qt-gui`. QML is embedded as a Qt resource rather than being loaded from an arbitrary working directory.

The ViewModel is constructed on the Qt application thread and outlives the QML engine. It now owns a CallManager and exposes NOTIFY-enabled `registrationStatus`, `callActionsEnabled` and `lastError` properties. The dial pad edits only a local destination field; it does not issue network requests. The default ViewModel has no backend and continues to show a disconnected state. The injected `CallBackend` is non-owning and must outlive the manager. Both CallManager methods and observer notifications are restricted to its construction thread. A future PJSIP adapter must queue *sanitized* asynchronous callbacks onto the QObject thread (e.g., using `Qt::QueuedConnection`) and must not call back synchronously from a command dispatch.

## Verification strategy

For Commits 1–2, verify both independent build modes: existing non-Qt/PJSIP regression and opt-in Qt configuration. In the Qt build, `QtGuiShellSmokeTest` launches with `QT_QPA_PLATFORM=offscreen` and software rendering, checks that QML instantiates a destination field and that call actions are disabled, then exits automatically. Run the GUI manually to inspect the layout.

Commit 2 adds `CallManagerTest.*` to the existing default, Qt and PJSIP GTest suites. A fake backend verifies request acceptance, explicit asynchronous completion, invalid destinations, failure paths, observer snapshots and disconnected behavior without network access. The GUI smoke test still asserts an inert default backend. End-to-end GUI registration, two-way audio and RTP are *not* established by these tests; Phase 6 has separate CLI evidence.

## Design rationale

Keeping Qt optional prevents new desktop dependencies from breaking established headless and non-PJSIP builds. The ViewModel is a QObject contract rather than a wrapper around raw PJSUA2 pointers. A Qt-free facade reuses the established core state machine and can be tested with an injected fake backend. An unavailable default backend prevents a visually plausible prototype from claiming to be registered or able to place calls. A failed request never masquerades as a successful asynchronous event. Future commits must implement SIP lifecycle ownership and explicit callback marshaling before enabling UI actions.

## Phase boundary

Phase 6 owns the validated CLI RTP/audio runtime. Phase 7 will provide registration, dialing, incoming/active-call views and diagnostics through a C++ application interface. Commit 2 deliberately does not provide a production SIP adapter, credential management, call-media ownership or a GUI call button. Phase 8 will expand the threading and message-queue model; CallManager is currently thread-confined, not thread-safe.

## References

- `README_roadmap.md` (Phase 7 — Qt/QML GUI)
- `docs/phase7/README.md`
- `docs/architecture/audio-rtp.md`
- `docs/phase6/README.md`
- `src/gui/GuiViewModel.hpp`
- `src/application/CallManager.hpp`
- `tests/unit/test_call_manager.cpp`
