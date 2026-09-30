# Phase 7 Architecture — Qt/QML UI boundary

## Status

Commit 4 extends the Commit 3 retained SIP registration boundary with an opt-in **outgoing** call workflow and Phase 6 bidirectional audio bridge. Incoming-call answering, continued registration-health monitoring, QML RTP statistics and Phase 7 milestone closure are not yet part of this commit.

## Purpose

Maintain the separation between Qt/QML UI, testable Qt-free application state, threaded call orchestration and the already-verified PJSUA2 call/audio implementation. Do not expose PJSIP credentials, backend exception text or raw protocol objects to QML.

## Scope

The inert GUI and Qt-only test build do not create SIP sessions. Only the combined Qt+PJSIP binary started using explicit `--sip-register USER SERVER [PORT]` may register and call. A dialer provides outbound calls and hangup. Incoming SIP INVITEs and RTP diagnostics inside Qt are future work; the headless Phase 6 behavior is preserved.

## Architecture model

```text
QML (dial pad, Call, Hang up, status)
  -> GuiViewModel (QObject; Qt UI thread)
  -> CallManager (Qt-free state machine; same UI thread)
  -> CallBackend API
  -> QtRegistrationBackend (QObject UI facade + C++20 worker/command queue)
  -> RegistrationSession (fake-injectable Qt-free call/registration contract)
  -> PjsipRegistrationSession (only combined Qt+PJSIP build)
  -> existing SipOutgoingCallClient (same account for REGISTER and INVITE)
  -> existing PJSUA2 CallAudioBridge / audio devices
```

The command queue is single-session: `begin_outgoing_call` and `begin_hangup` accept/reject requests on the UI thread; actual PJSUA2 API operations occur sequentially on the worker. The GUI supplies a validated user/extension only; the worker uses the configured SIP server, port and transport to generate the URI.

## Runtime and lifecycle

`GuiViewModel` owns `CallManager` only on its Qt thread. The bridge owns a background `std::jthread`, which creates and retains one session through registration and later calls. Registration success and SIP/media state are posted to Qt using queued signal delivery; no synchronous backend callback can reenter CallManager. Worker polling for call progress is bounded (150 ms ticks); initial call setup has a 30-second deadline, and failure/no-answer requests termination. A call is reported `Connected` only when Phase 6 confirms SIP call and active audio media. Local/remote end waits for PJSIP disconnect and release of the call slot before `Ready`/redial.

The worker and PJSIP client must be destroyed after the QML engine and ViewModel. During shutdown the bridge requests worker stop; the worker requests hangup if needed and releases the account/endpoint on the same construction thread. On a fatal cleanup problem the bridge does not permit another call on that running session. The same PJSUA2 account prevents conflicting parallel endpoints.

## Verification strategy

Qt-only and Qt+PJSIP builds both run fake-session queued registration/call/termination tests, CallManager/core state tests and an inert/no-network GUI smoke test. Existing Phase 6 regression (formatting, static analysis, sanitizer/non-PJSIP/PJSIP suites) remains required. Separately, a manual Asterisk gate validates live REGISTER, outgoing INVITE, two-way audio, GUI Hang up, remote hangup, redial, and explicit rejected-call recovery. Fake tests cannot establish live audio or real RTP behavior.

## Design rationale

Reusing Phase 6 `SipOutgoingCallClient` preserves one SIP account, its audio-device setup and its callback-protected call wrapper. The new `release_disconnected_call()` gates redial on PJSUA2 call-slot cleanup, rather than destroying an active callback object. Recovering a failed call while registration is retained is a separate state event; a failed initial registration remains a registration retry. UI actions and queued events do not carry credentials or untrusted library error strings. The backend opt-in capability avoids enabling calls in inert/Qt-only modes.

## Phase boundary

Commit 4 intentionally does not implement incoming-call answer/reject, call hold/mute, device selection UI, continuous registration failure monitoring, or live RTP diagnostic panels. Until local manual Asterisk checks pass, the new outbound flow is a candidate implementation, not a verified production feature. Preserve the existing verified Phase 6 CLI tools and do not merge Phase 7 based solely on passing synthetic unit tests.

## References

- `README_roadmap.md`
- `docs/phase7/README.md`
- `docs/architecture/audio-rtp.md`
- `src/gui/QtRegistrationBackend.hpp`
- `src/gui/PjsipRegistrationSession.hpp`
- `src/application/CallManager.hpp`
- `src/pjsip/SipOutgoingCallClient.hpp`
- `tests/qt/test_qt_registration_backend.cpp`
