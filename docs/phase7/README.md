# Phase 7 — Qt/QML GUI

## Status

**Completed** — milestone tag `v0.8-qt-gui`. Accepted for the documented local Asterisk lab and merged to `main`.

The accepted scope includes registration, one active call, incoming/outgoing call handling, explicit Answer/Decline behavior, connected-call duration, two-way audio, and live RTP/audio diagnostics.

## Objective

Provide a Qt/QML desktop presentation layer over the validated PJSUA2 signaling and audio runtime while preserving clear ownership boundaries.

QML remains presentation-only. Application call state is Qt-free, and PJSUA2 endpoint/account/call/media/statistics work stays outside the Qt UI thread.

## Scope

Included in this phase:

- Qt-only preview mode with no network activity;
- SIP registration in the Qt+PJSIP build;
- one active incoming or outgoing call;
- Answer, Decline, local hangup, remote hangup, and remote cancel;
- registration reuse across sequential calls;
- connected-call duration;
- read-only codec/RTP/loss/jitter/RTT diagnostics.

Deliberately excluded:

- multiple accounts, call waiting, hold, or mute;
- audio-device selection UI and persistent call history;
- generalized concurrency infrastructure beyond the accepted Qt worker/session boundary.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| Optional GUI build | Provide Qt-only preview and Qt+PJSIP dialer | `CMakePresets.json`, `src/gui/Main.qml` |
| Application state | Keep call lifecycle semantics Qt-free | `src/application/CallManager.hpp` |
| Presentation adapter | Convert backend events into sanitized QML-visible state, duration, and diagnostics | `src/gui/GuiViewModel.hpp` |
| Worker/session boundary | Own and serialize registered PJSUA2 activity outside the UI thread | `src/gui/QtRegistrationBackend.hpp`, `src/gui/PjsipRegistrationSession.hpp` |
| SIP call control | Support registration reuse, incoming/outgoing calls, and explicit `603 Decline` | `src/pjsip/SipOutgoingCallClient.hpp` |
| Credentials | Obtain passwords through the existing hidden-stdin runtime flow | `src/gui/` |
| Verification | Exercise fake-session/Qt ownership and inherited SIP/media regressions | `tests/qt/test_qt_registration_backend.cpp` |
| Evidence | Retain sanitized GUI, call, Decline, and RTP results | `evidence/phase7/README.md` |

## Architecture and design

```text
QML
  -> GuiViewModel                  Qt UI thread
  -> CallManager                   Qt-free application state
  -> CallBackend
  -> QtRegistrationBackend         retained std::jthread
  -> RegistrationSession
  -> PjsipRegistrationSession
  -> SipOutgoingCallClient
       -> PJSUA2 endpoint/account/call
       -> Phase 6 audio + RTP diagnostics
```

The retained PJSUA2 session owns network, call, media, and statistics lifetime. Worker results are delivered back to the Qt owner thread rather than mutating UI state directly from the SIP worker.

Caller/codec presentation data is sanitized before reaching QML. Raw SIP headers, credentials, backend exception details, and PJSUA2 objects do not cross the UI boundary.

See `docs/architecture/qt-qml.md` for the full design.

## Build and run

Qt-only:

```bash
cmake --preset qt-gui-debug
cmake --build --preset qt-gui-debug --clean-first -j2
```

Qt + PJSIP:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake --preset qt-pjsip-gui-debug
cmake --build --preset qt-pjsip-gui-debug --clean-first -j2
```

Run the combined GUI with:

```text
--sip-register USER SERVER [PORT]
```

Supply the SIP password only through the existing hidden stdin path. Do not place credentials in argv, shell history, screenshots, evidence, or committed logs.

## Verification

Automated acceptance covered both Qt build modes and inherited Phase 5/6 regressions. The recorded final Phase 7 gate passed:

- selected Qt tests in Qt-only mode;
- selected Qt tests in Qt+PJSIP mode;
- inherited Phase 5/6 automated verification;
- documentation and diff-quality checks.

Manual live verification covered:

- registration;
- outgoing and incoming calls;
- Answer and explicit Decline;
- two-way audio;
- connected duration;
- local and remote hangup;
- remote cancel before answer;
- outgoing call reuse without re-registering;
- live RTP/audio diagnostics across sequential calls.

For Decline, the softphone sends SIP `603 Decline` to Asterisk. An earlier lab capture showed Asterisk mapping that result to `403 Forbidden` on the caller-facing leg; the post-closure dialplan correction now preserves `603 Decline` end-to-end with Q.850 cause 21. The earlier `403` remains historical evidence, not the final Phase 7 state.

## Evidence

Curated Phase 7 evidence is indexed at `evidence/phase7/README.md` and includes:

- automated verification;
- manual GUI/call lifecycle verification;
- sanitized SIP Decline mapping and final end-to-end `603` correction;
- representative RTP/audio diagnostics.

Do not commit SIP passwords, Digest authorization material, complete raw SIP/PJSIP transcripts, raw audio, unrelated peer identifiers, or packet captures containing voice payloads.

## Interview focus

Be able to explain:

- why QML does not call PJSUA2 directly;
- why `CallManager` remains Qt-free;
- why one retained worker/session owns live PJSUA2 work;
- how incoming/outgoing calls reuse the same registered account;
- how remote-disconnect races are handled;
- why Decline differs from Hang up;
- why duration starts at Connected;
- why diagnostics are observational rather than call-state inputs;
- why worker results return through queued Qt handoff.

See `docs/interview/phase7-questions.md`.

## Exit criteria

Phase 7 is accepted when:

- both Qt build modes compile;
- selected Qt tests and inherited Phase 5/6 gates pass;
- real registration succeeds;
- outgoing/incoming calls provide two-way audio;
- Answer/Decline/local/remote/cancel paths recover to a reusable registered state;
- connected duration behaves deterministically;
- live RTP diagnostics appear, update, clear after hangup, and repopulate for a later call;
- sanitized evidence and documentation are complete.

The annotated `v0.8-qt-gui` tag is the Phase 7 release milestone.

## References

- `ROADMAP.md`
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

Phase 8 makes the broader concurrency model explicit through queue/worker primitives, stress testing, ThreadSanitizer evidence, and deeper Qt/PJSUA2 ownership/lifetime review while preserving the accepted Phase 7 UI boundary.
