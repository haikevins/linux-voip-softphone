# Phase 7 — Qt/QML GUI

## Status

Phase 7 acceptance is complete for the documented local Asterisk lab. Commits 1–7 delivered the optional Qt/QML shell, Qt-free application layer, live SIP registration, outgoing and incoming calls, explicit Answer/Decline behavior, connected-call duration, and live RTP/audio diagnostics. The final implementation checkpoint before this closure documentation is Commit 7 (`abb0b27`). Phase 7 release identifier: `v0.8-qt-gui`; the annotated tag is published and Phase 7 was merged to `main` by PR #8 at `2bd8740373a02bc38009df9a031b8863b7e0d824`.

## Objective

Provide a usable Qt/QML desktop presentation layer on top of the already validated PJSUA2 signaling and audio runtime while preserving clear ownership boundaries. QML must remain presentation-only: call state belongs to the application layer, and all PJSUA2 endpoint/account/call/media/statistics work remains outside the Qt UI thread.

## Scope

The combined `qt-pjsip-gui-debug` application can register a SIP account, place and receive one active call, answer or decline incoming calls, hang up locally, recover from remote hangup/cancel, reuse the same registration for later calls, display connected-call duration, and show a read-only live RTP/audio snapshot. Qt-only/preview builds remain network-free.

The accepted Phase 7 scope is intentionally single-account/single-call. It does not include call waiting, multiple accounts, hold, mute, audio-device selection UI, persistent call history, continuous registration-health events, or a general-purpose cross-thread queue. Those are future design work rather than closure blockers.

## Deliverables

- Optional Qt/QML build path and dialer shell.
- Qt-free `CallManager` application layer that owns user-visible call state transitions.
- `GuiViewModel` Qt presentation adapter with bounded/sanitized properties.
- Retained `QtRegistrationBackend` worker/session boundary for live PJSUA2 work.
- Secure runtime SIP registration using the existing hidden stdin password flow.
- Outgoing SIP calls with connected audio and recovery after rejection/disconnect.
- Incoming SIP calls with sanitized caller display, Answer, Decline, remote cancel, and local/remote hangup.
- Explicit `603 Decline` from the softphone on the callee-side SIP leg.
- Connected-call duration that starts only after connection and resets on teardown.
- Read-only live RTP/audio diagnostics: codec, RX/TX packet counters, packet loss, jitter, and RTCP RTT.
- Qt fake-session tests that keep Qt-only verification network-free while exercising worker-to-UI handoff.
- Curated Phase 7 evidence under `evidence/phase7/`.

## Architecture and design

```text
QML
  -> GuiViewModel                     (Qt UI thread)
  -> CallManager                      (Qt-free call/application state)
  -> CallBackend
  -> QtRegistrationBackend            (UI facade + retained std::jthread)
  -> RegistrationSession
  -> PjsipRegistrationSession
  -> SipOutgoingCallClient
       -> PJSUA2 endpoint/account/call
       -> Phase 6 audio bridge and RTP diagnostics
```

The historical class name `SipOutgoingCallClient` remains even though the retained client now owns both incoming and outgoing calls for the single account. Phase 7 deliberately avoids introducing a second endpoint/account/media owner.

Incoming caller text and codec text are sanitized before they become QML properties. Raw SIP URIs, SIP headers, backend exception strings, credentials, and PJSUA2 objects do not cross the UI boundary. Diagnostics are observational only and cannot drive call-state transitions.

## Build and run

Build Qt-only and Qt+PJSIP separately:

```bash
cmake --preset qt-gui-debug
cmake --build --preset qt-gui-debug --clean-first -j2

export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake --preset qt-pjsip-gui-debug
cmake --build --preset qt-pjsip-gui-debug --clean-first -j2
```

Run the production GUI with `--sip-register USER SERVER [PORT]`. Supply the SIP password through the existing hidden stdin path. Do not place credentials in argv, shell history, screenshots, evidence, or committed logs.

## Verification

The final Commit 7 automated gate passed on the tested workstation:

```text
Documentation structure: PASS
Qt-only selected tests: 18/18 PASS
Qt+PJSIP selected tests: 18/18 PASS
Phase 6 regression groups: 45/45, 133/133, 133/133, 88/88 PASS
Phase 5 automated verification: PASS
Focused inherited suites: 4/4 and 3/3 PASS
Phase 6 automated verification: PASS
git diff --check: PASS
```

The live manual gate passed registration, incoming Answer, two-way audio, connected duration, local/remote hangup reset, remote cancel before answer, outgoing call without re-registering, and live RTP diagnostics across sequential calls. The final RTP diagnostic run showed PCMU at 8 kHz, increasing RX/TX counters, zero reported packet loss in the observed snapshots, finite jitter/RTT values, panel clearing after hangup, and fresh diagnostics on the next call.

For Decline semantics, the softphone returned `603 Decline` to Asterisk. Asterisk then generated `403 Forbidden` toward the Redmi caller (`Q.850` cause 21); this is a B2BUA/dialplan call-leg mapping rather than the Qt client sending 403.

## Evidence

Curated, sanitized Phase 7 evidence is indexed by `evidence/phase7/README.md`:

- `automated-verification.txt` records the observed final test counts.
- `manual-verification.txt` records the accepted GUI/call lifecycle checks.
- `sip-decline.txt` records the sanitized two-leg `603` to `403` mapping observed at Asterisk.
- `rtp-diagnostics.txt` records representative sanitized PCMU/RTP diagnostic snapshots from the final manual run.

Do not commit SIP passwords, Digest authorization material, complete raw SIP/PJSIP transcripts, raw audio, unrelated peer identifiers, or packet captures containing voice payloads.

## Interview focus

Be able to explain why QML does not call PJSUA2 directly; why `CallManager` is Qt-free; why one retained worker/session owns PJSUA2; how incoming/outgoing calls share the same registered account; how the remote-hangup race was handled; why Decline needs an explicit SIP response; why duration is tied to Connected rather than Ringing; why RTP diagnostics are non-fatal observational data; and why UI-thread delivery uses queued handoff instead of direct cross-thread mutation. See `docs/interview/phase7-questions.md`.

## Exit criteria

Acceptance is complete for the documented local lab: both Qt build modes compile; selected Qt tests pass in both modes; the full inherited Phase 5/6 regression gate passes; real registration succeeds; outgoing and incoming calls provide two-way audio; Answer/Decline/local/remote/cancel paths recover to a reusable registered state; call duration behaves deterministically; and live RTP diagnostics appear, update, clear, and repopulate on a later call. The evidence applies to the tested revision and environment and is not a guarantee for other SIP servers, devices, or networks.

The annotated `v0.8-qt-gui` tag and the Phase 7 merge are now present in Git refs. Branch deletion remains a separate repository-maintenance operation and should not be inferred from this document.

## References

- `README_roadmap.md`
- `docs/architecture/qt-qml.md`
- `docs/architecture/audio-rtp.md`
- `docs/interview/phase7-questions.md`
- `evidence/phase7/README.md`
- `src/application/CallManager.hpp`
- `src/gui/GuiViewModel.hpp`
- `src/gui/QtRegistrationBackend.hpp`
- `src/gui/PjsipRegistrationSession.hpp`
- `src/pjsip/SipOutgoingCallClient.hpp`
- `tests/qt/test_qt_registration_backend.cpp`

## Next phase

Phase 8 focuses on the broader multithreading model: explicit event handoff/queueing, stress and lifetime tests, and ThreadSanitizer-oriented evidence. That work should preserve the accepted Phase 7 QML/application/PJSUA2 ownership boundaries instead of retroactively expanding the GUI milestone.
