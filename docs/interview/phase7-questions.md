# Phase 7 Interview Review — Qt/QML GUI

Use the repository implementation, documentation, tests, and evidence as the basis for the answers. Explain the engineering decision and the observed behavior; do not memorize isolated one-line definitions.

## Review scope

- Qt/QML presentation versus application and PJSUA2 ownership
- retained registration/session lifetime
- incoming and outgoing call handling
- Answer, Decline, hangup, and remote-cancel semantics
- connected-call duration
- live RTP/audio diagnostics
- worker-to-UI thread handoff
- accepted Phase 7 boundary

## Questions and answers

### UI and ownership boundaries

#### Why does QML not call PJSUA2 directly?

PJSUA2 owns network, callback, call, media, and statistics lifecycles that must not become UI-thread responsibilities. QML binds only to sanitized properties and actions. Live library work stays behind the retained backend/session boundary and presentation updates return to the Qt owner thread.

#### Why keep `CallManager` Qt-free?

It keeps application call semantics independent of Qt and PJSIP. State transitions can be unit-tested without a Qt event loop, SIP credentials, or network access, while `GuiViewModel` remains a thin presentation adapter.

#### Why reuse one retained PJSUA2 account/session for incoming and outgoing calls?

The registered SIP account is already the authoritative identity/contact. Reusing one retained session avoids duplicate endpoint/account ownership, preserves registration across calls, and makes sequential incoming/outgoing behavior representative of one softphone session.

### Call lifecycle and SIP behavior

#### What was the remote-hangup race and how is it handled?

A call can reach confirmed/media-active state and disconnect before the worker publishes every intermediate UI event. The backend uses the cumulative observed state and final disconnect instead of manufacturing a failure merely because a transient Connected publication was overtaken.

#### Why is Decline separate from Hang up?

Declining an unanswered INVITE is a final response to call setup. Hanging up an established dialog is teardown. Phase 7 therefore sends explicit SIP `603 Decline` on the softphone callee-side leg.

#### Why did the peer display `403 Forbidden` even though the softphone sent `603 Decline`?

Asterisk is a B2BUA and owns separate call legs. The observed trace showed Asterisk receiving `603 Decline` from the softphone leg and generating `403 Forbidden` toward the other leg with Q.850 cause 21. The responses belong to different SIP dialogs.

#### When should call duration start?

Only after the application reaches Connected. Ringing/setup time is not conversation time. Duration resets during teardown so the next call starts from zero.

### Diagnostics and thread handoff

#### Why are RTP diagnostics not part of the call state machine?

Codec, counters, loss, jitter, and RTT are observability data. A temporary statistics-read failure must not disconnect a healthy call or force a call-state transition.

#### Why sample diagnostics on the worker and deliver them with queued Qt handoff?

The worker owns PJSUA2 objects and their lifetime. Reading statistics there avoids cross-thread library access. Queued delivery ensures `GuiViewModel` and `CallManager` are updated on their Qt owner thread.

#### What evidence shows Phase 7 did not regress Phase 6?

The accepted automated gate passed selected Qt tests in both Qt-only and Qt+PJSIP builds and then passed the inherited Phase 5/6 regressions. Separate live calls confirmed two-way audio and current RTP diagnostics through the GUI path.

## Phase boundary

Phase 7 is the accepted single-account/single-call Qt/QML milestone. It does not claim support for multiple accounts, call waiting, mute/hold, audio-device selection UI, persistent history, or generalized concurrency infrastructure.

## References

- `docs/phase7/README.md`
- `docs/architecture/qt-qml.md`
- `docs/architecture/audio-rtp.md`
- `evidence/phase7/README.md`
- `src/application/CallManager.hpp`
- `src/gui/QtRegistrationBackend.hpp`
- `tests/qt/test_qt_registration_backend.cpp`
