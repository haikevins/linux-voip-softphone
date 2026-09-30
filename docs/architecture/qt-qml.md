# Phase 7 Architecture — Qt/QML UI boundary

## Status

Phase 7 architecture is accepted for the documented single-account/single-call lab. QML remains presentation-only, `CallManager` remains Qt-free, and one retained backend worker/session owns all live PJSUA2 endpoint/account/call/media/statistics operations. The accepted implementation includes registration, outgoing/incoming calls, Answer/Decline, call duration, recovery/reuse, and read-only RTP/audio diagnostics.

## Purpose

Define a Qt/QML boundary that can present a real bidirectional SIP softphone without transferring protocol or media ownership into the UI layer. The design must keep call state testable without Qt/PJSIP, isolate PJSUA2 lifetime to one worker-owned session, sanitize protocol-derived text, and deliver UI updates on the Qt thread.

## Scope

The architecture covers one registered SIP account and one tracked call at a time. It includes outgoing and incoming call control, connected-call duration, explicit incoming Decline, remote cancellation/disconnection recovery, and live codec/RTP/RTCP diagnostics.

It does not cover multiple accounts, call waiting, hold, mute, audio-device selection UI, persistent call history, continuous registration-health events, or a general-purpose cross-thread event queue. These are future extensions and must not be inferred from the Phase 7 acceptance result.

PJSIP `remoteUri`, codec text, backend exception text, and credentials are treated as untrusted/library-side data. Caller display is reduced to a validated dialable user part or `Unknown caller`; codec text is restricted to a short token-like label; raw headers/URIs and credentials never become QML state.

## Architecture model

```text
QML (dialer + call controls + duration + RTP panel)
  |
  v
GuiViewModel (QObject, Qt UI thread)
  |
  +--> CallManager (Qt-free application/call state)
  |
  +<-- primitive/sanitized backend events and RtpDiagnosticsSnapshot
        ^
        |
QtRegistrationBackend (retained std::jthread + queued Qt delivery)
  |
RegistrationSession
  |
PjsipRegistrationSession
  |
SipOutgoingCallClient
  |
PJSUA2 endpoint/account/call + Phase 6 audio/RTP diagnostics
```

`CallManager` owns user-visible call lifecycle semantics. RTP diagnostics are deliberately not part of the state machine because packet counters/jitter/RTT are observational data, not transition inputs.

## Runtime and lifecycle

The backend retains one worker thread. The worker creates the production registration session, which creates one PJSUA2 endpoint/account and reuses that account across sequential incoming/outgoing calls. PJSUA2 operations never run directly from QML or mutate `GuiViewModel` from the worker thread.

Registration and call operations publish sanitized events back to Qt with queued delivery. Incoming calls first publish ringing/caller state; Answer sends the normal success response, while Decline uses a dedicated call path that sends SIP `603 Decline` rather than reusing established-call hangup semantics. Remote cancel or disconnect returns the application to the registered Ready state.

A connected-call timer is owned by the UI-facing layer and starts only after the call reaches Connected. It stops/resets on teardown so a later call cannot inherit stale duration.

After media connection, the worker samples the active audio stream's RTP diagnostics at most once per second. Diagnostic read failures are best-effort/non-fatal. Teardown clears the diagnostic snapshot before final call completion so stale values cannot persist into the next call.

## Verification strategy

Qt-only fake sessions validate registration/call state transitions, incoming/outgoing paths, explicit Answer/Decline behavior, thread-affine delivery, deterministic clearing, and diagnostic property mapping without linking PJSIP or touching the network. The combined Qt+PJSIP build compile-checks the production session and live PJSUA2 mapping. The inherited Phase 6 gate protects the underlying SIP/audio/RTP implementation.

The accepted final automated run recorded 18/18 selected tests in Qt-only mode and 18/18 in Qt+PJSIP mode, plus the full Phase 5/6 regression gate. Live Asterisk tests then verified actual registration, incoming/outgoing two-way audio, local/remote/cancel recovery, duration, `603 Decline` on the softphone-to-Asterisk leg, registration reuse, and RTP panel update/clear/repopulate behavior.

## Design rationale

Keeping PJSUA2 off the Qt thread prevents library callbacks and blocking/network work from becoming UI-thread responsibilities. A Qt-free `CallManager` makes application-state tests fast and deterministic. One retained production session avoids duplicate endpoint/account ownership and allows registration reuse between calls.

Dedicated Decline semantics avoid conflating an unanswered INVITE with teardown of an established dialog. The Asterisk lab demonstrated that a B2BUA can map the callee-side `603 Decline` into a different caller-side final response; therefore protocol evidence must identify the call leg being observed.

The RTP projection contains only primitive counters and bounded labels. Sampling is intentionally low-frequency and non-fatal so observability cannot become a call-availability dependency.

## Phase boundary

Phase 7 stops at the accepted single-account/single-call GUI boundary. Mute/hold, device selection, multiple accounts/calls, persistent history, continuous registration-health events, generalized event queues, stress testing, and ThreadSanitizer-focused concurrency evidence are not Phase 7 completion requirements.

Phase 8 may generalize cross-thread event flow, but it should preserve the core ownership rule established here: QML presents state; application code owns semantics; the backend owns live SIP/media library interaction.

## References

- `README_roadmap.md`
- `docs/phase7/README.md`
- `docs/architecture/audio-rtp.md`
- `docs/interview/phase7-questions.md`
- `evidence/phase7/README.md`
- `src/application/CallManager.hpp`
- `src/gui/GuiViewModel.hpp`
- `src/gui/QtRegistrationBackend.hpp`
- `src/gui/PjsipRegistrationSession.hpp`
- `src/pjsip/SipOutgoingCallClient.hpp`
- `tests/qt/test_qt_registration_backend.cpp`
