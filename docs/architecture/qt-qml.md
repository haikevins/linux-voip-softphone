# Phase 7 Architecture — Qt/QML UI boundary

## Status

Commit 5 extends the retained Commit 4 SIP account from outgoing-only GUI calling to **incoming call notification, Answer and Decline**. The same PJSUA2 endpoint/account remains responsible for REGISTER, outgoing INVITE, incoming INVITE and audio. QML never owns a SIP object.

## Purpose

Define the Qt/QML boundary for a bidirectional desktop softphone while preserving the existing application/core separation and the single retained PJSUA2 account introduced by earlier Phase 7 commits. Incoming SIP callbacks are converted into worker-owned events and delivered to the Qt thread before application state or QML is updated.

## Scope

Commit 5 covers incoming-call notification, a sanitized caller label, Answer, Decline, remote cancel, local/remote hangup and subsequent calls without re-registration. It preserves Commit 4 outgoing calling and explicit failure recovery. It does not add call waiting, multiple accounts, mute/hold, device selection, call timers, continuous registration-health events or in-GUI RTP statistics.

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

While the state machine is `Ringing`, `CallManager` separately records whether the ring is incoming. Only an incoming ring exposes `answer_actions_enabled`. `request_answer()` asks the backend to answer but intentionally leaves the state as `Ringing` until PJSIP confirms both SIP connection and active audio; repeated Answer requests are disabled during that pending period. Outgoing `RemoteRinging` never enables Answer.

Decline reuses the existing hangup request. The state moves to `Terminating`, the worker terminates the early incoming dialog, and the queued end event returns the manager to `Ready` while retaining registration.

## Runtime and lifecycle

`QtRegistrationBackend` owns one `std::jthread`. The worker creates `PjsipRegistrationSession`, which creates one PJSUA2 endpoint/account. After registration the worker alternates between command checks and a bounded incoming-offer wait. No PJSUA2 call operation runs on the Qt thread.

For an incoming INVITE, the account callback creates one tracked call wrapper and sends `180 Ringing`. The worker publishes only a sanitized caller label. Answer sends `200 OK`; connection is not published to QML until the existing call result reports both confirmed SIP state and active audio media. Decline/local hangup and remote disconnect both wait for the call to disconnect and for the PJSUA2 call slot to be released before another call is allowed.

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
Ready -> Ringing -> Terminating -> Ready
```

Outgoing behavior remains Commit 4 behavior:

```text
Ready -> Calling -> Ringing? -> Connected -> Terminating -> Ready
```

A call request rejected before connection may still enter `Failed` and use explicit **Clear error** recovery without re-registering. A normal incoming remote cancel/decline path must return to `Ready`, not `Failed`.

## Verification strategy

Qt-only fake sessions verify that incoming notification is queued onto the UI thread, Answer is explicit, Decline does not lose registration, remote hangup returns to `Ready`, and the existing outgoing/recovery behavior is unchanged. The combined Qt+PJSIP build additionally compile-checks the production adapter and the extended PJSUA2 client. Full Phase 6 verification protects existing registration/outgoing/incoming CLI and RTP behavior.

Live Asterisk verification remains a separate manual gate. It must prove incoming ringing on the ThinkPad GUI, Answer, two-way audio, local and remote hangup, Decline, remote cancel, and a subsequent call without re-registration. Synthetic tests do not establish that Asterisk routing or real audio works.

## Design rationale

The Phase 6 class name `SipOutgoingCallClient` predates incoming GUI support. Commit 5 extends it instead of creating a second registered PJSUA2 account, because two independent account/session owners would make routing, contact registration and lifetime rules ambiguous. Renaming can be done later as a mechanical refactor.

Incoming protocol text is reduced to a small validated caller label before reaching QML so the presentation layer does not become a parser or trust boundary for arbitrary SIP data. The worker/thread boundary also keeps PJSUA2 lifetime and blocking operations outside the UI thread while leaving `CallManager` deterministic and Qt-free.

## Phase boundary

Commit 5 does not add mute/hold, audio-device selection UI, call timer, continuous registration-health events, RTP statistics in QML, call waiting, multiple accounts or Phase 8's general-purpose queueing model. Those concerns stay separate from the incoming-call ownership change.

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
