# Phase 7 Architecture — Qt/QML UI boundary

## Status

In progress. Commit 1 introduces an optional Qt6 GUI shell with a deliberately inert C++ ViewModel. Runtime SIP integration is not implemented by this commit.

## Purpose

Establish a Qt/QML boundary that can display VoIP status and request call operations in later commits without exposing PJSUA2 objects or running SIP work from QML.

## Scope

This document covers the GUI target, its read-only initial ViewModel, and planned boundaries with the existing C++ core and PJSIP components. SIP/account lifecycle, incoming-call handling, audio routing, and diagnostics integration are future Phase 7 work.

## Architecture model

The roadmap's target dependency direction is:

```text
QML view
    |
    v
QObject ViewModel (UI-thread ownership)
    |
    v
CallManager (future application facade)
    |
    v
SipEngine / existing PJSUA2 adapters (future integration)
```

Commit 1 implements only the first two layers. `src/gui/Main.qml` reads `GuiViewModel` properties; the `Call` action remains disabled. `src/app/qt_gui_main.cpp` creates the Qt application, ViewModel and QQmlApplicationEngine. The GUI links to `voip::core` but does not instantiate any PJSUA2 endpoint.

## Runtime and lifecycle

`ENABLE_QT_GUI=OFF` leaves the established CLI and Phase 6 targets unchanged. When enabled, CMake requires Qt6 Quick and QuickControls2 and builds `voip-qt-gui`. QML is embedded as a Qt resource rather than being loaded from an arbitrary working directory.

The ViewModel is constructed on the Qt application thread and outlives the QML engine. It exposes an explicit disconnected status. The dial pad edits only a local destination field; it does not issue network requests. Future backend callbacks must be delivered to the GUI thread before mutating QObject-observable state.

## Verification strategy

For Commit 1, verify both independent build modes: existing non-Qt/PJSIP regression and opt-in Qt configuration. In the Qt build, `QtGuiShellSmokeTest` launches with `QT_QPA_PLATFORM=offscreen` and software rendering, checks that QML instantiates a destination field and that call actions are disabled, then exits automatically. Run the GUI manually to inspect the layout.

End-to-end registration, two-way audio and RTP remain covered by the completed Phase 6 evidence; they are not implied by passing the GUI smoke test.

## Design rationale

Keeping Qt optional prevents new desktop dependencies from breaking established headless and non-PJSIP builds. The ViewModel is a QObject contract rather than a wrapper around raw PJSUA2 pointers. An inert initial backend prevents a visually plausible prototype from falsely claiming to be registered or able to place calls. Subsequent commits can add a testable application facade before wiring real SIP events.

## Phase boundary

Phase 6 owns the validated CLI RTP/audio runtime. Phase 7 will provide registration, dialing, incoming/active-call views and diagnostics through a C++ application interface. Phase 8 will expand the threading and message-queue model; Commit 1 makes no unsupported thread-safety claims.

## References

- `README_roadmap.md` (Phase 7 — Qt/QML GUI)
- `docs/phase7/README.md`
- `docs/architecture/audio-rtp.md`
- `docs/phase6/README.md`
- `src/gui/GuiViewModel.hpp`
