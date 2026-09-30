# Phase 7 — Qt/QML GUI

## Status

In progress. Commit 1 delivered the Qt dialer shell, Commit 2 the Qt-free `CallManager`, Commit 3 live SIP registration, and Commit 4 outgoing SIP calls with two-way audio. **Commit 5 adds incoming-call notification, Answer and Decline on the same retained PJSUA2 account.** Continuous registration-health monitoring, mute/device controls, in-GUI RTP diagnostics and Phase 7 closure remain future work.

## Objective

Make the desktop GUI a bidirectional softphone without introducing a second SIP endpoint/account. Incoming PJSIP callbacks are converted into sanitized worker events, queued onto the Qt thread, and then applied through `CallManager`. QML never receives credentials, raw PJSIP objects or raw SIP URIs.

## Scope

The combined `qt-pjsip-gui-debug` application started with `--sip-register USER SERVER [PORT]` can register, place outgoing calls, receive an INVITE, show the caller extension, answer it, decline it and return to `Ready to call` after local or remote hangup. Qt-only and GUI-preview modes remain network-free.

An incoming caller is displayed only as a validated dialable user part (1–64 digits / `*` / `#` / `+`). Any remote URI that cannot be reduced safely is shown as `Unknown caller`. The GUI does not expose arbitrary SIP header text.

## Deliverables

- `CallManager` adds an explicit asynchronous answer request and tracks whether a ringing state belongs to an incoming call. Outgoing ringing never enables the Answer action.
- `QtRegistrationBackend` polls for incoming offers on the retained worker, publishes a sanitized `incomingCall(QString)` event, and serializes Answer, Decline/Hang up, connect, disconnect and cleanup with outgoing calls.
- `PjsipRegistrationSession` exposes incoming offer polling and answering while retaining the same account created for registration/outgoing calls.
- The Phase 6 `SipOutgoingCallClient` is extended as the retained GUI call session: its account handles `onIncomingCall`, sends `180 Ringing`, accepts with `200 OK`, rejects a second simultaneous INVITE with `486 Busy Here`, and reuses the existing Phase 6 audio bridge.
- QML adds an incoming-call panel with **Answer** and **Decline** controls. After connection, the normal active-call **Hang up** control is used.
- Unit/fake-session tests cover explicit answer, decline without re-registration, incoming call → connected → remote hangup, and the existing Commit 4 outgoing/recovery paths.

## Architecture and design

```text
QML
  -> GuiViewModel                (Qt UI thread)
  -> CallManager                 (Qt-free application state)
  -> CallBackend
  -> QtRegistrationBackend       (UI facade + retained std::jthread)
  -> RegistrationSession
  -> PjsipRegistrationSession
  -> SipOutgoingCallClient       (retained Phase 6 PJSUA2 endpoint/account)
       -> Account::onIncomingCall
       -> PJSUA2 Call / audio bridge
```

The historical class name `SipOutgoingCallClient` is retained in this commit to avoid a broad Phase 6 API rename; in the GUI path it now owns both directions for the single retained account. A later refactor may rename that implementation without changing behavior.

The backend checks for an incoming offer while idle using bounded waits. Once an offer exists, it stays in the same worker call loop until the call is answered/declined/disconnected and the PJSUA2 call slot is safely released. A second incoming INVITE while another call object exists is rejected as busy.

## Build and run

Build Qt-only and Qt+PJSIP separately. The production GUI keeps the existing hidden-password stdin flow; do not put a real SIP password in argv, logs or screenshots.

```bash
cmake --preset qt-gui-debug
cmake --build --preset qt-gui-debug --clean-first -j2

export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake --preset qt-pjsip-gui-debug
cmake --build --preset qt-pjsip-gui-debug --clean-first -j2
```

Start the combined build with the registered extension and SIP server using the existing `--sip-register USER SERVER [PORT]` interface, supplying the password through stdin rather than exposing it in shell arguments.

## Verification

Run the Qt-only tests, the combined Qt+PJSIP tests and the inherited Phase 6 gate:

```bash
ctest --preset qt-gui-debug \
    -R 'CallManagerTest|QtRegistrationBackendTest|QtGuiShellSmokeTest' \
    --output-on-failure

ctest --preset qt-pjsip-gui-debug \
    -R 'CallManagerTest|QtRegistrationBackendTest|QtGuiShellSmokeTest' \
    --output-on-failure

./scripts/phase6/verify.sh
```

The fake Qt tests never use a real SIP server. Passing them proves queued state/ownership behavior only; it does not prove live incoming signaling or audio.

The manual incoming-call gate uses the same local Asterisk account setup already verified for Commit 4. After the GUI shows `Registered`, place a call **from the Redmi/Linphone endpoint to the ThinkPad extension** and verify the GUI shows `Incoming call` with the sanitized caller extension.

Test both paths separately: on the Answer path, verify `Connected (audio active)`, two-way audio, Qt hangup and remote hangup, with each call returning to `Ready to call` without re-registering. On the Decline path, decline before answering and verify the Redmi stops ringing, Qt returns to `Ready to call`, and a later incoming or outgoing call still works without re-registration. Also test remote cancel before Answer; Qt must return to `Ready to call` rather than `Request failed`.

## Evidence

Record only summarized PASS/FAIL, automated test counts, GUI-visible state and sanitized extension/status. Do not commit SIP passwords, Digest headers, packet captures containing credentials, raw audio or unredacted SIP logs. Incoming peer text shown in QML is deliberately reduced to a dialable extension or `Unknown caller`.

Automated fake-session results are evidence for state/thread behavior only. Live Asterisk signaling and real two-way audio require the separate manual gate.

## Interview focus

Be able to explain why Commit 5 reuses one retained PJSUA2 account for registration and both call directions; how `Account::onIncomingCall` is serialized through the worker before reaching Qt; why Answer remains pending until SIP plus active audio are confirmed; how early Decline/remote cancel differ from a connected remote hangup; why only one tracked call is supported; and why raw SIP URI text is sanitized before QML sees it.

## Exit criteria

Commit 5 is complete only when both Qt build variants and inherited Phase 6 regression pass, followed by manual Asterisk checks for incoming ring, Answer, two-way audio, Qt hangup, remote hangup, Decline, remote cancel and another call on the same registration. Phase 7 remains open for diagnostics/remaining GUI work and milestone closure.

## References

- `README_roadmap.md` — Phase 7 Qt/QML GUI
- `docs/architecture/qt-qml.md`
- `docs/architecture/audio-rtp.md`
- `src/application/CallManager.hpp`
- `src/gui/QtRegistrationBackend.hpp`
- `src/gui/PjsipRegistrationSession.hpp`
- `src/pjsip/SipOutgoingCallClient.hpp`
- `tests/unit/test_call_manager.cpp`
- `tests/qt/test_qt_registration_backend.cpp`

## Next phase

Add the remaining Phase 7 active-call/diagnostic presentation without changing the verified SIP account ownership model. Keep continuous registration-health monitoring and broader concurrency work separate unless explicitly designed and tested.
