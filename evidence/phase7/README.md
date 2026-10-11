# Phase 7 Evidence — Qt/QML GUI

## Status

Phase 7 is accepted for the documented local lab and published as the `v0.8-qt-gui` milestone.

The final evidence includes the post-closure 2026-10-01 corrections: caller-leg Decline is preserved end-to-end as SIP `603 Decline`, and an outgoing remote rejection after Ringing returns the GUI to Ready without a false failure state.

## Verification scope

Evidence covers the Qt/QML shell, Qt-free application state, retained worker/session ownership, registration, one active incoming/outgoing call, Answer/Decline, local/remote hangup, remote cancel, connected duration, registration reuse, two-way audio, RTP/audio diagnostics, and the two post-closure regressions.

## Environment

The accepted lab used Ubuntu, Qt 6, PJPROJECT/PJSIP 2.17, local Asterisk 20.6, and a phone/Linphone peer. Credentials were supplied through the hidden runtime flow and are not retained.

## Automated verification

The original Phase 7 gate passed selected Qt-only and Qt+PJSIP tests plus inherited Phase 5/6 regressions.

After the outgoing-remote-rejection hotfix, the rebuilt Qt/PJSIP debug suite passed 135/135, including `QtRegistrationBackendTest.RemoteRejectAfterRingingIsNotReportedAsFailure`.

## Manual verification

The operator confirmed registration, incoming Answer, outgoing calls, two-way audio, connected duration, local/remote hangup, remote cancel, registration reuse, and live RTP/audio diagnostics.

For final Decline behavior, the softphone returns SIP `603 Decline` to Asterisk and the corrected lab dialplan preserves `603 Decline` on the caller-facing leg with Q.850 cause 21. The earlier caller-side `403 Forbidden` capture remains in `sip-decline.txt` as superseded historical evidence.

## Artifacts

- `automated-verification.txt` — original and post-closure automated results.
- `manual-verification.txt` — original and post-closure manual results.
- `sip-decline.txt` — pre-fix mapping plus final end-to-end `603 Decline` correction.
- `rtp-diagnostics.txt` — representative sanitized RTP/audio observations.

## Results

Phase 7 acceptance passed for the stated local environment. The final recorded state includes end-to-end `603 Decline` preservation and normal recovery from remote rejection after Ringing without re-registration.

## Security and redaction

Do not retain SIP credentials, Digest material, SDP/SRTP keys, complete raw SIP/PJSIP transcripts, raw audio, or packet captures. Keep only the sanitized facts needed to support the verification.

## Reproduction

Build/test the Qt-only and Qt+PJSIP configurations described in `docs/phase7/README.md`, run the inherited Phase 6 gate, then perform the documented live GUI call/diagnostics matrix. Do not archive unredacted runtime logs.

## References

- `docs/phase7/README.md`
- `docs/architecture/qt-qml.md`
- `docs/architecture/audio-rtp.md`
- `docs/interview/phase7-questions.md`
- `tests/qt/test_qt_registration_backend.cpp`
