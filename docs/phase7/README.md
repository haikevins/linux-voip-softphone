# Phase 7 — Qt/QML GUI

## Status

In progress. Commit 1 delivered a Qt dialer shell; Commit 2 delivered the Qt-free `CallManager`; Commit 3 delivered live initial SIP registration. **Commit 4 adds opt-in outgoing SIP calls and hangup through the SAME registered PJSUA2 account, using the Phase 6 call/audio implementation.** Incoming-call answering, long-running registration-health monitoring, in-GUI RTP diagnostics and Phase 7 closure are not included.

## Objective

Expose a real outbound SIP calling workflow from the Qt/QML desktop dialer without putting PJSUA2 objects, account credentials, or blocking network operations on the GUI thread. Preserve the existing headless SIP/RTP tests and optional Qt-only smoke mode.

## Scope

The combined `qt-pjsip-gui-debug` application started with `--sip-register USER SERVER [PORT]` enables registration and, **after confirmation**, a Call button. The GUI sends only an extension (1–64 dialable characters) to `CallManager`; the worker assembles the SIP URI from the already configured server, runs the Phase 6 `SipOutgoingCallClient` and connects audio using the existing Phase 6 bridge. Qt-only and ordinary GUI preview modes remain network-free and cannot place a call. No incoming-call answer control is provided.

## Deliverables

- `QtRegistrationBackend` retains one worker and serializes registration, dialing, ringing, connect, hangup and cleanup. A `RegistrationSession` provides an injectable, Qt-free, call-capable seam; existing registration-only fake sessions remain usable.
- `PjsipRegistrationSession` delegates to Phase 6 `SipOutgoingCallClient` rather than creating a second endpoint. `Connected` is published only after both SIP confirmation and active audio media.
- `SipOutgoingCallClient::release_disconnected_call()` waits for call-slot cleanup before allowing another call on the same account; its `stop()` is idempotent.
- `CallManager` tracks whether initial registration succeeded. An explicitly recovered call failure returns to `Ready` without requesting registration again; failed initial registration still resets to `Idle`.
- QML exposes destination, **Call**, **Hang up**, sanitized call status, active peer, and a contextual **Clear error** control. The default GUI smoke test remains inert.
- The existing `CallManager` unit tests and Qt fake-session tests are expanded to cover retained registration, successful call, local hangup, remote hangup, redial and explicit recovery after a rejected call. Test expectations do not require network access.

## Architecture and design

QML → `GuiViewModel` → `CallManager` → `QtRegistrationBackend` → `RegistrationSession` → `PjsipRegistrationSession` → existing Phase 6 `SipOutgoingCallClient` / PJSUA2. The backend worker owns exactly one PJSUA2 endpoint/account for registration and outbound calls. `CallManager` is strictly Qt-thread-owned; all worker completion signals are queued to Qt. QML never accesses the SIP account, password or PJSUA2 objects. Remote disconnect returns the call to `Ready`, while rejected calls publish a generic error and need an explicit user reset.

During the active-call phase, the worker checks for Hang up approximately every 150 ms. Initial ringing display is opportunistic; lack of a separate `Ringing...` event is not necessarily a failure. Initial call setup has a 30-second timeout, and a disconnected call wrapper must be safely released before redial. In an unrecoverable cleanup failure, outbound calling remains disabled for that application session; close/restart the app instead of attempting another call.

## Build and run

On Ubuntu 24.04, install Qt6 headers and the QML runtime modules as needed:

```bash
sudo apt install qt6-base-dev qt6-declarative-dev \
    qml6-module-qtquick qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts qml6-module-qtqml-workerscript \
    qml6-module-qtquick-templates
```

Build the inert Qt-only target separately from the production Qt+PJSIP target:

```bash
cmake --preset qt-gui-debug
cmake --build --preset qt-gui-debug -j
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake --preset qt-pjsip-gui-debug
cmake --build --preset qt-pjsip-gui-debug -j
```

For an optional controlled test with your local Asterisk, read the SIP password from a hidden terminal prompt. Do not embed a real password in shell history, logs or screenshots:

```bash
(
    set -euo pipefail
    set +x
    read -r -p 'SIP extension: ' SIP_EXT
    read -r -p 'Asterisk host/IP: ' SIP_HOST
    read -r -s -p 'SIP password: ' SIP_PASS
    printf '\n'
    trap 'unset SIP_PASS' EXIT
    printf '%s\n' "$SIP_PASS" | \
        ./build/qt-pjsip-gui-debug/src/voip-qt-gui \
            --sip-register "$SIP_EXT" "$SIP_HOST" 5060
)
```

Click **Register**, wait for `Registered`, dial a known local test extension, check the SIP and audio status, then test **Hang up** and a second call. The command above uses the existing validated PJSIP account input flow; QML does not collect credentials.

## Verification

Before any live call, run both builds and the inherited regression gate:

```bash
ctest --preset qt-gui-debug \
    -R 'CallManagerTest|QtRegistrationBackendTest|QtGuiShellSmokeTest' \
    --output-on-failure
ctest --preset qt-pjsip-gui-debug \
    -R 'CallManagerTest|QtRegistrationBackendTest|QtGuiShellSmokeTest' \
    --output-on-failure
./scripts/phase6/verify.sh
```

**Manual gate is separate**: confirm registration with Asterisk, place a controlled outgoing call to the known test endpoint, hear audio in both directions, hang up from GUI, repeat using remote hangup, verify the UI returns to `Ready`, and try a second call on the same registration. Test a rejected/unanswered call and explicit **Clear error** recovery. Validate mute/volume or audio-device settings only in future work; this commit reuses Phase 6 defaults. No manual outgoing call is considered proven until this separate gate passes on the user's machine.

## Evidence

Record only summarized PASS/FAIL, test counts, GUI visible state and sanitized SIP status. Do not commit or share SIP passwords, credentials in environment dumps, authentication headers, raw audio, packet captures or full SIP logs. A successful automated fake-session run is not evidence of a live INVITE or two-way audio.

## Interview focus

Explain how the single retained registered account is reused for INVITEs; how a Qt queued event prevents PJSIP callbacks from updating QML off-thread; why `Connected` requires an active audio-media indication; how call-slot cleanup gates redial; and why registration recovery and call recovery are different state transitions.

## Exit criteria

Commit 4 requires compilation of both Qt-only and combined Qt/PJSIP modes, network-free core and fake Qt tests, the inherited full Phase 6 automated gate and **manual** Asterisk registration/outgoing audio/hangup/redial checks. No commit or push is justified by documentation or synthetic tests alone. Phase 7 remains open until incoming-call UI and remaining planned diagnostics are implemented and separately verified.

## References

- `README_roadmap.md` — Phase 7 / `v0.8-qt-gui`
- `docs/architecture/qt-qml.md`
- `docs/architecture/audio-rtp.md`
- `src/application/CallManager.hpp`
- `src/gui/QtRegistrationBackend.hpp`
- `src/gui/PjsipRegistrationSession.hpp`
- `src/pjsip/SipOutgoingCallClient.hpp`
- `tests/qt/test_qt_registration_backend.cpp`

## Next phase

Add incoming-call notification and accept/reject UI on the same PJSUA2 account with explicit worker-thread ownership. Extend the Qt diagnostics panel and network/registration-health checks only after separate design and tests.
