# Phase 7 Interview Review — Qt/QML softphone boundary

## Review scope

Review the accepted Phase 7 design: Qt/QML presentation, application-state separation, retained PJSUA2 worker/session, incoming/outgoing call lifecycle, explicit SIP Decline semantics, connected-call duration, and live RTP/audio diagnostics. Answers should distinguish UI behavior from SIP/media ownership and should identify which thread/layer owns each responsibility.

## Questions and answers

**Why does QML not call PJSUA2 directly?**  
PJSUA2 owns network, callback, call, media, and statistics lifecycles that must not become UI-thread responsibilities. QML only binds to sanitized properties/actions. The backend serializes live library work on its retained worker and delivers presentation updates to the Qt thread.

**Why keep `CallManager` Qt-free?**  
It isolates call/application semantics from framework and protocol details. Registration/call state transitions can be unit-tested without Qt event loops, PJSIP, credentials, or network access, while `GuiViewModel` remains a thin presentation adapter.

**Why reuse one retained PJSUA2 account/session for incoming and outgoing calls?**  
The SIP account registration is already the authoritative identity/contact. Reusing it avoids duplicate endpoint ownership, preserves registration between calls, and makes sequential incoming/outgoing tests representative of a real softphone session.

**What was the remote-hangup race and how is it handled?**  
A call can reach confirmed/media-active state and disconnect before the UI worker publishes an intermediate Connected event. The backend treats the observed cumulative connected/media state followed by disconnect as a successful call completion rather than fabricating a failure.

**Why is Decline separate from Hang up?**  
Declining an unanswered INVITE is a final response to call setup, while hanging up an established dialog is teardown. Phase 7 sends explicit SIP `603 Decline` on the softphone's callee-side leg instead of relying on a generic hangup call path.

**Why did Redmi display `403 Forbidden` even though the softphone sent `603 Decline`?**  
Asterisk acts as a B2BUA with separate call legs. The trace showed Asterisk receiving `603 Decline` from extension 1002, then generating `403 Forbidden` toward extension 1001 with Q.850 cause 21. The two responses belong to different SIP dialogs/legs.

**When should call duration start?**  
Only after the application reaches Connected. Ringing/setup time is not connected conversation time. The duration is reset at teardown so a later call starts from zero.

**Why are RTP diagnostics not part of the call state machine?**  
Codec/counters/loss/jitter/RTT are observability data. A temporary statistics-read failure must not disconnect a healthy call or create a state transition. The worker therefore publishes a small best-effort snapshot independently of call-state semantics.

**Why sample diagnostics on the worker and deliver them with queued Qt handoff?**  
The worker owns PJSUA2 objects and their lifetime. Reading statistics there avoids cross-thread library access. Queued delivery ensures `GuiViewModel` is mutated on the Qt thread instead of from the SIP worker.

**What evidence shows Phase 7 did not regress Phase 6?**  
The final gate passed the selected Qt tests in both Qt-only and Qt+PJSIP builds and then passed the inherited Phase 5/6 regression suites. Live calls separately confirmed two-way audio and current RTP statistics through the GUI path.

**What remains for Phase 8?**  
General-purpose event queueing/handoff, deeper stress/lifetime testing, and ThreadSanitizer-oriented concurrency evidence. Phase 8 should generalize concurrency without moving SIP/media ownership into QML.

## Phase boundary

These answers describe the accepted Phase 7 single-account/single-call design. They do not claim support for call waiting, multiple accounts, mute/hold, device selection UI, persistent history, or generalized concurrency infrastructure.

## References

- `docs/phase7/README.md`
- `docs/architecture/qt-qml.md`
- `docs/architecture/audio-rtp.md`
- `evidence/phase7/README.md`
- `src/application/CallManager.hpp`
- `src/gui/QtRegistrationBackend.hpp`
- `tests/qt/test_qt_registration_backend.cpp`
