# Phase 7 Architecture — Qt/QML UI boundary

## Status

Commit 7 builds on the retained bidirectional SIP account with **read-only live RTP/audio diagnostics in QML**. The same PJSUA2 endpoint/account remains responsible for REGISTER, outgoing INVITE, incoming INVITE, media and statistics. QML never owns a SIP or media object.

## Purpose

Define the Qt/QML boundary for a bidirectional desktop softphone while preserving the application/core separation and single retained PJSUA2 account. Commit 7 extends that boundary with a small diagnostics projection so operational RTP information can be observed without exposing protocol/library objects to the presentation layer.

## Scope

Commit 7 preserves registration, outgoing/incoming calls, Answer/Decline, local/remote hangup and the call-duration UI. It adds a live connected-call panel for codec, RX/TX packets, packet loss, receive jitter and RTCP round-trip time. It does not add call waiting, multiple accounts, mute/hold, device selection, continuous registration-health events or persistent call history.

PJSIP `remoteUri` and codec text are treated as protocol/library text rather than trusted QML content. The caller remains reduced to a validated dialable user part or `Unknown caller`; the codec label is restricted to a short ASCII token-like form before publication. Raw SIP URIs, headers, backend exception text and credentials never cross the GUI boundary.

## Architecture model

```text
QML (dial pad + call controls + diagnostic panel)
  |
  v
GuiViewModel (QObject, Qt UI thread)
  |
  +--> CallManager (call/application state)
  |
  +<-- sanitized RtpDiagnosticsSnapshot values
        ^
        |
QtRegistrationBackend (retained worker + queued Qt signals)
  |
RegistrationSession
  |
PjsipRegistrationSession
  |
SipOutgoingCallClient
  |
Pjsua2RtpDiagnostics -> pj::Call stream/RTCP statistics
```

`CallManager` remains confined to the Qt thread and continues to own the user-visible call lifecycle. RTP diagnostics are deliberately not inserted into the call state machine because they are observational data, not state-transition inputs.

The worker converts the production PJSUA2 diagnostic object into a small transport-neutral `RtpDiagnosticsSnapshot`. Fake sessions can return deterministic snapshots without linking PJSIP, which keeps Qt-only verification network-free.

## Runtime and lifecycle

`QtRegistrationBackend` owns one `std::jthread`. The worker creates `PjsipRegistrationSession`, which creates one PJSUA2 endpoint/account. No PJSUA2 call or statistics operation runs on the Qt thread.

After `ConnectedWithMedia`, each active call loop samples diagnostics at most once per second. A successful sample is posted with `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` and then emitted as primitive Qt-friendly values. The ViewModel stores only the latest snapshot.

A diagnostic read is best-effort. An exception from the statistics path is swallowed at the telemetry boundary and does not change `Connected`, invoke Hang up, or mark the call failed. The next scheduled sample may succeed. This prevents observability from becoming a call-availability dependency.

Call teardown emits `rtpDiagnosticsCleared()` before the final call-ended/failed notification. The ViewModel resets its snapshot, so the panel cannot display stale counters on the next call. A new call receives a fresh snapshot from its own active stream.

Only the first active audio stream is projected because current Phase 7 supports one tracked call and the existing Phase 6 media path negotiates one active audio stream. Multi-stream presentation belongs with a future multi-media design.

## Verification strategy

Qt-only fake sessions verify that diagnostic sampling happens on the retained worker, delivery occurs on the Qt thread, ViewModel properties contain the expected deterministic snapshot, and teardown clears the snapshot. The GUI smoke test verifies the diagnostic panel exists in QML while remaining hidden in preview mode. Existing call lifecycle tests continue to protect registration, incoming/outgoing flows and recovery.

The combined Qt+PJSIP build compile-checks the real `SipOutgoingCallClient::rtp_diagnostics()` mapping. Full Phase 6 verification protects the underlying PJSUA2/RTCP extraction and existing CLI behavior.

Live Asterisk verification must establish a connected call and observe RX/TX counters changing over time. Codec should match the negotiated audio stream; loss, jitter and RTT must remain finite/non-negative. After either local or remote hangup, the panel must disappear; a later call on the same registration must publish a new snapshot.

## Design rationale

PJSUA2 diagnostics already existed before the GUI. Reusing that layer avoids duplicating RTP/RTCP calculations in QML or introducing packet parsing into the UI. The backend performs the read because it already owns PJSUA2 lifetime and serialization.

The projection contains primitive counters and bounded labels only. This keeps QML simple, prevents arbitrary protocol text from becoming presentation state, and makes the fake-session contract easy to test. Sampling once per second is frequent enough for human-visible diagnostics without turning the UI into a high-frequency telemetry consumer.

## Phase boundary

Commit 7 does not add mute/hold, audio-device selection UI, continuous registration-health events, persistent RTP history, packet capture, call waiting, multiple accounts or Phase 8's general-purpose queueing model. The panel is live observational state only and is cleared when the call ends.

## References

- `README_roadmap.md`
- `docs/phase7/README.md`
- `docs/architecture/audio-rtp.md`
- `src/gui/GuiViewModel.hpp`
- `src/gui/QtRegistrationBackend.hpp`
- `src/gui/PjsipRegistrationSession.hpp`
- `src/pjsip/Pjsua2RtpDiagnostics.hpp`
- `src/pjsip/SipOutgoingCallClient.hpp`
- `tests/qt/test_qt_registration_backend.cpp`
