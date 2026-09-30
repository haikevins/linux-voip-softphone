# Phase 7 — Qt/QML GUI

## Status

In progress. Commit 1 delivered the Qt dialer shell, Commit 2 the Qt-free `CallManager`, Commit 3 live SIP registration, Commit 4 outgoing SIP calls with two-way audio, and Commit 5 incoming calls. **Commit 6 separates SIP Decline from generic hangup and adds an active-call elapsed timer.** Continuous registration-health monitoring, mute/device controls, in-GUI RTP diagnostics and Phase 7 closure remain future work.

## Objective

Polish the verified bidirectional desktop softphone without changing its single-account ownership model. Commit 6 makes incoming rejection explicit through every layer and gives the user a simple connected-call duration indicator. QML still never receives credentials, raw PJSIP objects or raw SIP URIs.

## Scope

The combined `qt-pjsip-gui-debug` application started with `--sip-register USER SERVER [PORT]` can register, place outgoing calls, receive an INVITE, show the caller extension, answer it, explicitly decline it with SIP `603 Decline`, show elapsed connected time, and return to `Ready to call` after local or remote hangup. Qt-only and GUI-preview modes remain network-free.

An incoming caller is displayed only as a validated dialable user part (1–64 digits / `*` / `#` / `+`). Any remote URI that cannot be reduced safely is shown as `Unknown caller`. The GUI does not expose arbitrary SIP header text.

## Deliverables

- `CallManager` adds `request_decline()` plus a dedicated `decline_actions_enabled` snapshot flag so incoming rejection is no longer routed through generic Hang up.
- `QtRegistrationBackend` serializes a distinct decline command on the retained worker, mutually exclusive with Answer/Hang up.
- `PjsipRegistrationSession` forwards Decline to the retained PJSUA2 call client.
- `SipOutgoingCallClient::decline_incoming_call()` sends `603 Decline`; second simultaneous INVITEs still receive `486 Busy Here`, while Answer remains `200 OK`.
- `GuiViewModel` exposes `callConnected`; QML uses it to show/reset an `mm:ss` elapsed timer only while audio-connected state is active.
- Unit/fake-session tests verify the explicit decline path, no generic hangup on Decline, retained registration, and connected-state timer gating while preserving Commit 5 call flows.

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

The historical class name `SipOutgoingCallClient` remains to avoid a broad Phase 6 API rename; in the GUI path it owns both directions for the single retained account. Commit 6 only adds an explicit incoming-decline operation to that retained owner.

The backend checks for an incoming offer while idle using bounded waits. Once an offer exists, it stays in the same worker call loop until the call is answered/declined/disconnected and the PJSUA2 call slot is safely released. Answer, Decline and Hang up are serialized as separate commands; Decline maps specifically to `603`. A second incoming INVITE while another call object exists is rejected as busy.

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

Test both paths separately: on the Answer path, verify `Connected (audio active)`, two-way audio, the duration label advances once per second, Qt hangup and remote hangup, with each call returning to `Ready to call` and the timer resetting. On the Decline path, decline before answering and verify the Redmi stops ringing, Qt returns to `Ready to call`, and a later incoming or outgoing call still works without re-registration. Also test remote cancel before Answer; Qt must return to `Ready to call` rather than `Request failed`. If exact SIP semantics are being evidenced, confirm the generated final response is `603 Decline` from protocol logs rather than relying only on the caller UI wording.

## Evidence

Record only summarized PASS/FAIL, automated test counts, GUI-visible state and sanitized extension/status. Do not commit SIP passwords, Digest headers, packet captures containing credentials, raw audio or unredacted SIP logs. Incoming peer text shown in QML is deliberately reduced to a dialable extension or `Unknown caller`.

Automated fake-session results are evidence for state/thread behavior only. Live Asterisk signaling and real two-way audio require the separate manual gate.

## Interview focus

Be able to explain why the GUI reuses one retained PJSUA2 account for registration and both call directions; why Answer, Decline and Hang up are separate application/backend commands; why `603 Decline` is sent only for an unanswered incoming offer; why the QML duration counter starts from application `Connected` rather than INVITE/Ringing time; why only one tracked call is supported; and why raw SIP URI text is sanitized before QML sees it.

## Exit criteria

Commit 6 is complete only when both Qt build variants and inherited Phase 6 regression pass, followed by manual Asterisk checks for incoming ring, Answer, two-way audio, connected-time display/reset, Qt hangup, remote hangup, explicit Decline, remote cancel and another call on the same registration. Phase 7 remains open for diagnostics/remaining GUI work and milestone closure.

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

Add the remaining Phase 7 diagnostic presentation (notably in-GUI RTP/audio information) without changing the verified SIP account ownership model. Keep continuous registration-health monitoring and broader concurrency work separate unless explicitly designed and tested.
