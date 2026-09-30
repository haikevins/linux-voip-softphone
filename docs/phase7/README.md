# Phase 7 — Qt/QML GUI

## Status

In progress. Commit 1 established the optional Qt dialer shell; Commit 2 introduced the Qt-free `CallManager` and its testable lifecycle contract. **Commit 3 adds opt-in initial SIP registration**, retaining the production SIP session after successful registration. Real GUI calls, incoming calls, two-way audio and RTP diagnostics are not yet connected.

## Objective

Provide a Linux Qt/QML softphone UI on top of the verified Phase 6 SIP/RTP runtime, without leaking PJSIP implementation objects or session ownership into QML. Connect real SIP functions in independently testable increments.

## Scope

Commit 3 connects the registration UI to the existing Phase 6 `SipRegistrationClient` through `CallManager`, `QtRegistrationBackend` and `PjsipRegistrationSession`. Its production capability is **initial registration only**. The inert GUI remains network-free; a Qt-only build can test the bridge without linking PJSIP. Calling, incoming events, media and continuous registration monitoring are outside the commit.

## Deliverables

- `QtRegistrationBackend` implements the `CallBackend` interface using a C++20 `std::jthread` worker and replaceable `RegistrationSession` factory.
- `PjsipRegistrationSession` wraps the existing Phase 6 `SipRegistrationClient` only when **both** Qt and PJSIP are enabled; it does not create a new SIP protocol implementation.
- A queued Qt signal delivers the registration result to the UI-thread-owned `GuiViewModel` and `CallManager`. No PJSUA2 objects or raw backend exception text reach QML.
- QML provides a Register button and sanitized status. The registration-only backend reports `supports_calls() == false`, so the Call button remains disabled even after registration succeeds.
- The `qt-pjsip-gui-debug` preset builds Qt and PJSIP together. Qt-only and CLI configurations remain available.
- Fake-session tests exercise worker ownership, queued result delivery, retained registration, failure and retry; core tests cover registration-only call capability.

## Architecture and design

The dependency boundary is QML → `GuiViewModel` → `CallManager` → `QtRegistrationBackend` → `PjsipRegistrationSession` → existing `SipRegistrationClient`. The ViewModel owns the application state on the UI thread while the registration session runs on a dedicated worker. Completion reaches the UI through a queued signal; the SIP session is retained after successful registration, then released on its worker thread during shutdown.

The default GUI remains disconnected. With a registration-only backend, `CallManager` can transition to `Ready` after a confirmed registration but must still reject call operations. A generic failure message prevents raw PJSIP exceptions from reaching the UI. See `docs/architecture/qt-qml.md` for ownership, threading and the phase boundary.

## Build and run

Install Qt6 development packages and the QML runtime modules on Ubuntu 24.04 if required:

```bash
sudo apt install qt6-base-dev qt6-declarative-dev \
    qml6-module-qtquick qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts qml6-module-qtqml-workerscript \
    qml6-module-qtquick-templates
```

Point pkg-config at the already verified PJSIP/PJSUA2 2.17 installation, then configure the independent variants:

```bash
cmake --preset qt-gui-debug
cmake --build --preset qt-gui-debug -j

export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake --preset qt-pjsip-gui-debug
cmake --build --preset qt-pjsip-gui-debug -j
```

After automated verification, optionally launch the combined GUI for a controlled Asterisk registration check. The password is read from stdin, not from a shell command argument, QML or a project file. Replace the placeholders with the locally configured extension and Asterisk host:

```bash
(
    set -euo pipefail
    read -r -s -p 'SIP password: ' VOIP_SIP_PASSWORD
    printf '\n'
    trap 'unset VOIP_SIP_PASSWORD' EXIT
    printf '%s\n' "$VOIP_SIP_PASSWORD" | \
        ./build/qt-pjsip-gui-debug/src/voip-qt-gui \
        --sip-register YOUR_EXTENSION YOUR_ASTERISK_HOST 5060
)
```

Click **Register** to initiate registration. Do not paste credentials or capture private SIP details in shared logs.

## Verification

Run the network-free tests in both build variants and preserve the inherited Phase 6 regression gate:

```bash
ctest --preset qt-gui-debug \
    -R 'CallManagerTest|QtRegistrationBackendTest|QtGuiShellSmokeTest' \
    --output-on-failure
ctest --preset qt-pjsip-gui-debug \
    -R 'CallManagerTest|QtRegistrationBackendTest|QtGuiShellSmokeTest' \
    --output-on-failure
./scripts/phase6/verify.sh
```

The offscreen smoke test never constructs a SIP account or makes a network request, even in a combined build. The separate manual gate checks that GUI status moves from `Not registered` through `Registering...` to `Registered` **only after** backend confirmation, and independently verifies registration in local Asterisk. Check that the Call button remains disabled. Report failure or timeout using the generic UI status; retry only after explicitly requesting another registration.

## Evidence

Automated results should be retained as sanitized test counts and PASS/FAIL summaries. Keep the manual Asterisk registration result separate from automated test evidence. Do not include SIP passwords, authentication headers, raw packet captures or other private data in Git or shared logs. This documentation describes required evidence; it does not by itself establish that a manual registration test has passed.

## Interview focus

Explain why `CallManager` treats registration as pending until the backend reports success; how the queued Qt result preserves UI-thread ownership; why a successful PJSIP session must remain alive; and why a registration-only backend must still disable the Call button. Be able to distinguish initial REGISTER confirmation from ongoing network/account health.

## Exit criteria

For Commit 3, require clean Qt-only and combined Qt/PJSIP builds, core and queued-bridge tests, an inert offscreen smoke test and the full inherited Phase 6 automated regression. A controlled live Asterisk REGISTER confirmation is a separate manual acceptance gate and must be reported separately, not inferred from automated tests. Phase 7 as a whole additionally requires actual GUI calls, incoming/active-call UI, two-way audio and RTP diagnostics.

## References

- `README_roadmap.md` — Phase 7 / `v0.8-qt-gui`
- `docs/architecture/qt-qml.md`
- `docs/architecture/audio-rtp.md`
- `docs/phase6/README.md`
- `src/application/CallManager.hpp`
- `tests/unit/test_call_manager.cpp`
- `tests/qt/test_qt_registration_backend.cpp`

## Next phase

The next Phase 7 commit should extend the same account/session ownership boundary to real calls and media rather than starting a separate PJSIP endpoint in QML. Continue to keep Qt-only tests isolated from live SIP traffic and retain the inherited Phase 6 regression gate.
