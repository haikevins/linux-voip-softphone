# Phase 7 Architecture — Qt/QML UI boundary

## Status

Commit 6 builds on the retained bidirectional SIP account with **explicit SIP Decline semantics and active-call duration presentation**. The same PJSUA2 endpoint/account remains responsible for REGISTER, outgoing INVITE, incoming INVITE and audio. QML never owns a SIP object.

## Purpose

Define the Qt/QML boundary for a bidirectional desktop softphone while preserving the existing application/core separation and the single retained PJSUA2 account introduced by earlier Phase 7 commits. Incoming SIP callbacks are converted into worker-owned events and delivered to the Qt thread before application state or QML is updated.

## Scope

Commit 6 preserves the Commit 5 incoming/outgoing call lifecycle and adds two focused UX/semantic improvements: the GUI shows a local elapsed timer only while media is connected, and Decline is a distinct application/backend command that sends SIP `603 Decline` instead of reusing generic hangup. It does not add call waiting, multiple accounts, mute/hold, device selection, continuous registration-health events or in-GUI RTP statistics.

PJSIP's `remoteUri` is treated as untrusted protocol text. `PjsipRegistrationSession` extracts only the `sip:`/`sips:` user part before `@` and accepts it only when it is 1–64 dialable characters (`0-9`, `*`, `#`, `+`). Otherwise QML receives `Unknown caller`. Raw SIP URIs, headers, backend exception text and credentials never cross the GUI boundary.

## Architecture model

```text
QML (dial pad + incoming/active call controls)
  |
  v
GuiViewModel (QObject, Qt UI thread)
  |
  v
CallManager (Qt-free application state)
  |
  v
CallBackend
  |
  v
QtRegistrationBackend (QObject facade + retained worker)
  |
  v
RegistrationSession
  |
  v
PjsipRegistrationSession (Qt+PJSIP build only)
  |
  v
SipOutgoingCallClient (single retained PJSUA2 endpoint/account)
  |
  +--> outgoing Call::makeCall
  +--> Account::onIncomingCall
  +--> existing Phase 6 audio bridge
```

`CallManager` remains confined to the Qt thread. An incoming SIP callback is not allowed to mutate it directly. The worker publishes `incomingCall(QString)` via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`, and `qt_gui_main.cpp` translates that queued signal into `CallManager::on_incoming_call()`.

While the state machine is `Ringing`, `CallManager` separately records whether the ring is incoming. Only an incoming ring exposes `answer_actions_enabled` and `decline_actions_enabled`. `request_answer()` asks the backend to answer but intentionally leaves the state as `Ringing` until PJSIP confirms both SIP connection and active audio; repeated Answer/Decline requests are disabled during that pending period. Outgoing `RemoteRinging` never enables either incoming-call action.

Decline is now explicit end-to-end: QML calls `declineCall()`, `CallManager::request_decline()` moves the call toward `Terminating`, the Qt worker invokes `RegistrationSession::decline_call()`, and the production PJSUA2 adapter sends `603 Decline`. Generic Hang up remains reserved for cancelling an outgoing attempt or ending an answered call.

## Runtime and lifecycle

`QtRegistrationBackend` owns one `std::jthread`. The worker creates `PjsipRegistrationSession`, which creates one PJSUA2 endpoint/account. After registration the worker alternates between command checks and a bounded incoming-offer wait. No PJSUA2 call operation runs on the Qt thread.

For an incoming INVITE, the account callback creates one tracked call wrapper and sends `180 Ringing`. The worker publishes only a sanitized caller label. Answer sends `200 OK`; Decline sends `603 Decline`. Connection is not published to QML until the existing call result reports both confirmed SIP state and active audio media. Decline/local hangup and remote disconnect all wait for the call to disconnect and for the PJSUA2 call slot to be released before another call is allowed.

Only one tracked call is supported. A second incoming INVITE while a call object already exists receives `486 Busy Here`. This is deliberate single-call behavior, not call waiting.

Incoming accepted call:

```text
Ready
  -> IncomingCallReceived("1001") -> Ringing
  -> request_answer()             -> Ringing (answer pending)
  -> CallConnected                -> Connected
  -> remote/local end             -> Terminating -> Ready
```

Incoming declined/cancelled call:

```text
Ready -> Ringing -> request_decline() -> Terminating -> Ready
```

The call-duration display is intentionally presentation-only. `GuiViewModel::callConnected()` exposes whether the application state is `Connected`; a QML `Timer` increments once per second only while that property is true and resets when the call leaves `Connected`. It is not persisted and is not presented as SIP/RTP timing evidence.

Outgoing behavior remains Commit 4 behavior:

```text
Ready -> Calling -> Ringing? -> Connected -> Terminating -> Ready
```

A call request rejected before connection may still enter `Failed` and use explicit **Clear error** recovery without re-registering. A normal incoming remote cancel/decline path must return to `Ready`, not `Failed`.

## Verification strategy

Qt-only fake sessions verify that incoming notification is queued onto the UI thread, Answer and Decline are distinct commands, Decline does not use the generic hangup path or lose registration, the connected-state property drives the timer boundary, remote hangup returns to `Ready`, and the existing outgoing/recovery behavior is unchanged. The combined Qt+PJSIP build additionally compile-checks the production adapter and the extended PJSUA2 client. Full Phase 6 verification protects existing registration/outgoing/incoming CLI and RTP behavior.

Live Asterisk verification remains a separate manual gate. It must prove incoming ringing on the ThinkPad GUI, Answer, two-way audio, visible duration growth while connected, local and remote hangup, explicit Decline, remote cancel, and a subsequent call without re-registration. The caller may render the final response with client-specific wording; protocol logging is the authoritative way to confirm a `603` if that evidence is needed. Synthetic tests do not establish that Asterisk routing or real audio works.

## Design rationale

The Phase 6 class name `SipOutgoingCallClient` predates incoming GUI support. Commit 6 continues to reuse it instead of creating a second registered PJSUA2 account, because two independent account/session owners would make routing, contact registration and lifetime rules ambiguous. Renaming can be done later as a mechanical refactor.

Incoming protocol text is reduced to a small validated caller label before reaching QML so the presentation layer does not become a parser or trust boundary for arbitrary SIP data. The worker/thread boundary also keeps PJSUA2 lifetime and blocking operations outside the UI thread while leaving `CallManager` deterministic and Qt-free.

## Phase boundary

Commit 6 does not add mute/hold, audio-device selection UI, continuous registration-health events, RTP statistics in QML, call waiting, multiple accounts or Phase 8's general-purpose queueing model. The new elapsed timer is local UI state only; richer call-history/diagnostic timing remains out of scope.

## References

- `README_roadmap.md`
- `docs/phase7/README.md`
- `docs/architecture/audio-rtp.md`
- `src/application/CallManager.hpp`
- `src/gui/GuiViewModel.hpp`
- `src/gui/QtRegistrationBackend.hpp`
- `src/gui/PjsipRegistrationSession.hpp`
- `src/pjsip/SipOutgoingCallClient.hpp`
- `tests/unit/test_call_manager.cpp`
- `tests/qt/test_qt_registration_backend.cpp`
