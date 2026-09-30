# Phase 7 — Qt/QML GUI

## Status

In progress. Commit 1 provides the optional Qt6/QML dialer shell. Commit 2 adds a Qt-free CallManager facade and observable ViewModel properties. SIP registration, calling and audio remain intentionally disconnected in the shipping GUI.

## Objective

Build the desktop softphone interface on top of the verified Phase 6 SIP/RTP runtime, without leaking PJSIP implementation objects or ownership into QML.

## Scope

The milestone roadmap calls for a registration view, dialer, incoming-call and active-call interfaces, and diagnostics. Commit 1 supplied a runnable GUI skeleton. Commit 2 establishes a testable application boundary with accepted/pending commands and explicit success/failure events, while keeping the real call button disabled until a production SIP backend is integrated.

## Deliverables

Commit 1 added the Qt6/QML shell and offscreen smoke test. Commit 2 adds `src/application/CallManager.*`, integrates its observable snapshots into `GuiViewModel`, and registers `tests/unit/test_call_manager.cpp` in the existing Qt-free core unit test target. There is still no production SIP adapter, credentials, real GUI calls, or newly claimed live runtime evidence.

## Architecture and design

The intended boundary is QML → ViewModel → CallManager → CallBackend interface → future SIP adapter. Commit 2 implements CallManager atop the existing core CallStateMachine. Accepted commands are distinct from asynchronous registration/call completion. `GuiViewModel` now has NOTIFY-enabled `registrationStatus`, `callActionsEnabled` and `lastError`. The default executable still injects no backend, reports disconnected and disables calls. QML does not import, create or call PJSUA2. See `docs/architecture/qt-qml.md` for component ownership and planned integration boundaries.

## Build and run

Install Qt6 development packages and the QML runtime modules used by this shell. On Ubuntu 24.04, package candidates include:

```bash
sudo apt install qt6-base-dev qt6-declarative-dev \
    qml6-module-qtquick qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts \
    qml6-module-qtqml-workerscript \
    qml6-module-qtquick-templates
```

Then configure and build the optional GUI without requiring PJSIP:

```bash
cmake --preset qt-gui-debug
cmake --build --preset qt-gui-debug -j
./build/qt-gui-debug/src/voip-qt-gui
```

The CLI and Phase 6 gates still work with their existing presets because `ENABLE_QT_GUI` defaults to `OFF`. The Qt-free CallManager also compiles in the default build. A GUI-only smoke test must not be mistaken for an actual SIP call.

## Verification

Check the legacy default build and, when Qt6 is installed, the opt-in GUI build:

```bash
./scripts/quality/verify-docs-structure.sh
cmake --preset qt-gui-debug
cmake --build --preset qt-gui-debug -j
ctest --preset qt-gui-debug -R '^QtGuiShellSmokeTest$' --output-on-failure
```

The smoke check runs offscreen, verifies that QML instantiated, finds the destination field and confirms that call actions are disabled. Also run the new facade test group with `ctest --preset qt-gui-debug -R "CallManagerTest" --output-on-failure` and repeat in the default build. Run the GUI manually to verify the disconnected shell renders. Run the existing `scripts/phase6/verify.sh` independently to check for regressions.

## Evidence

Do not claim Phase 7 runtime evidence until the above commands execute on the target ThinkPad. Future UI screenshots will be added to `evidence/phase7/` only after real functionality is implemented and identifying details are reviewed. Never commit SIP passwords, raw network captures or sensitive call details.

## Interview focus

Explain why Qt is optional, why the Qt-free facade is injected behind a CallBackend interface, why request acceptance is not SIP success, how the offscreen smoke test differs from end-to-end SIP testing, and how GUI-thread affinity constrains future asynchronous callbacks.

## Exit criteria

Commit 2 exits when the Qt-free CallManager tests pass in the default and Qt configurations, the inert GUI smoke test remains green, and Phase 6 verification does not regress. This is a design-layer milestone, not a working GUI call. Phase 7 as a whole exits only after actual SIP account registration, calling, incoming/active-call UI and diagnostics have been wired and verified end-to-end with sanitized evidence.

## References

- `README_roadmap.md` (Phase 7 plan and `v0.8-qt-gui` milestone)
- `docs/architecture/qt-qml.md`
- `docs/phase6/README.md`
- `docs/architecture/audio-rtp.md`
- `tests/CMakeLists.txt` (`QtGuiShellSmokeTest`, `CallManagerTest.*`)
- `src/application/CallManager.hpp`

## Next phase

The next Phase 7 commit should implement a production SIP adapter with secure, non-QML account configuration and careful PJSUA2 ownership, then marshal events to the Qt thread before enabling registration or call actions. Commit 2 only adds the testable application facade, not the adapter. Phase 8 will address broader multithreading requirements.
