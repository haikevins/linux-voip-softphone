# Phase 7 — Qt/QML GUI

## Status

In progress. Commit 1 provides an opt-in, runnable Qt6/QML dialer shell and a read-only C++ ViewModel. SIP registration, calling and audio are intentionally not connected to the GUI yet.

## Objective

Build the desktop softphone interface on top of the verified Phase 6 SIP/RTP runtime, without leaking PJSIP implementation objects or ownership into QML.

## Scope

The milestone roadmap calls for a registration view, dialer, incoming-call and active-call interfaces, and diagnostics. Commit 1 is limited to the dependency and application skeleton: launch a GUI, show an honest disconnected state, enter a destination using the dial pad, and keep real call operations disabled until the C++ application layer exists.

## Deliverables

Commit 1 adds an optional `ENABLE_QT_GUI` CMake option, `qt-gui-debug` configure/build/test presets, a `voip-qt-gui` application, the embedded QML screen, a read-only `GuiViewModel`, and one offscreen CTest smoke check. It also establishes the GUI architecture document. No credentials, live screenshots or new runtime evidence are included.

## Architecture and design

The planned boundary is QML → ViewModel → CallManager → SipEngine. Only the QML and initial ViewModel layers exist in this commit. The model exposes `registrationStatus` and `callActionsEnabled`; it reports disconnected and disables calls. QML does not import, create or call PJSUA2. See `docs/architecture/qt-qml.md` for component ownership and planned integration boundaries.

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

The CLI and Phase 6 gates still work with their existing presets because `ENABLE_QT_GUI` defaults to `OFF`. A GUI-only smoke test must not be mistaken for an actual SIP call.

## Verification

Check the legacy default build and, when Qt6 is installed, the opt-in GUI build:

```bash
./scripts/quality/verify-docs-structure.sh
cmake --preset qt-gui-debug
cmake --build --preset qt-gui-debug -j
ctest --preset qt-gui-debug -R '^QtGuiShellSmokeTest$' --output-on-failure
```

The smoke check runs offscreen, verifies that QML instantiated, finds the destination field and confirms that call actions are disabled. Also run the GUI manually to verify the shell renders. Run the existing `scripts/phase6/verify.sh` independently to check for regressions.

## Evidence

Do not claim Phase 7 runtime evidence until the above commands execute on the target ThinkPad. Future UI screenshots will be added to `evidence/phase7/` only after real functionality is implemented and identifying details are reviewed. Never commit SIP passwords, raw network captures or sensitive call details.

## Interview focus

Explain why Qt is optional, why QML sees a QObject ViewModel rather than PJSUA2, how Qt resource loading avoids runtime path dependencies, how the offscreen smoke test differs from end-to-end SIP testing, and how GUI-thread affinity will constrain future asynchronous callbacks.

## Exit criteria

Commit 1 exits when the default non-Qt build remains intact, the Qt GUI configures/builds, its QML view loads offscreen, manual shell rendering works, and existing verification stays green. Phase 7 as a whole exits only after actual SIP account registration, calling, incoming/active-call UI and diagnostics have been wired and verified end-to-end with sanitized evidence.

## References

- `README_roadmap.md` (Phase 7 plan and `v0.8-qt-gui` milestone)
- `docs/architecture/qt-qml.md`
- `docs/phase6/README.md`
- `docs/architecture/audio-rtp.md`
- `tests/CMakeLists.txt` (`QtGuiShellSmokeTest`)

## Next phase

The next Phase 7 commit should introduce a testable application facade with explicit asynchronous state and error reporting before connecting the existing PJSIP clients to the ViewModel. Phase 8 will address broader multithreading requirements.
