# Phase 7 Evidence — Qt/QML GUI

## Status

Phase 7 acceptance evidence is complete for the documented local lab. The final implementation checkpoint before closure is Commit 7 (`abb0b27`). Automated Qt-only/Qt+PJSIP tests and inherited Phase 5/6 regressions passed, and the operator confirmed the complete live GUI call/diagnostics gate. Release tagging and branch merge remain separate Git operations.

## Verification scope

Qt/QML shell behavior, Qt-free application state, retained worker/session ownership, SIP registration, outgoing and incoming calls, Answer/Decline, local/remote hangup, remote cancel before answer, connected-call duration, registration reuse, two-way audio, and live codec/RTP/RTCP diagnostic presentation.

The scope is one account and one active tracked call in the local Asterisk lab. It does not claim call waiting, multiple accounts, hold/mute, device selection UI, persistent call history, or general-purpose concurrency infrastructure.

## Environment

Ubuntu workstation with Qt 6, PJPROJECT/PJSIP 2.17, local Asterisk 20.6, and a Redmi phone running Linphone as the peer. The workstation used lab extension 1002 and the phone used lab extension 1001. Credentials were entered only at runtime through the hidden stdin flow and are not retained in evidence.

## Automated verification

The final Commit 7 gate passed documentation verification, 18/18 selected tests in the Qt-only build, 18/18 selected tests in the Qt+PJSIP build, and the inherited Phase 6 verification wrapper. The inherited results included Phase 5 verification plus observed groups of 45/45, 133/133, 133/133, 88/88, 4/4, and 3/3 tests. `git diff --check` also passed. See `automated-verification.txt`.

## Manual verification

The operator confirmed live registration, incoming Answer, outgoing call, two-way audio, duration start/increment/reset, local and remote hangup recovery, remote cancel before answer, Decline, registration reuse, and RTP diagnostics across sequential calls. The diagnostic panel appeared only during connected calls, RX/TX counters increased, codec/loss/jitter/RTT were displayed, teardown cleared the panel, and a later call repopulated it without re-registration.

For Decline, Asterisk logging showed the Qt/PJSUA2 endpoint returning `603 Decline`; Asterisk subsequently returned `403 Forbidden` toward the Redmi caller on the separate caller-side leg. See `sip-decline.txt`. Representative PCMU/RTP measurements from the final manual run appear in `rtp-diagnostics.txt`.

## Artifacts

- `automated-verification.txt`: final observed build/test/regression results.
- `manual-verification.txt`: accepted GUI and call lifecycle checks.
- `sip-decline.txt`: sanitized Asterisk observation of `603 Decline` on the callee leg and `403 Forbidden` on the caller leg.
- `rtp-diagnostics.txt`: representative sanitized RTP/audio snapshots from the final GUI diagnostic test.

Only curated summaries are retained. Full terminal transcripts, passwords, authorization headers, and raw packet/audio captures are deliberately excluded.

## Results

The recorded acceptance gates passed for the stated local configuration. Phase 7 demonstrates a functional Qt/QML softphone front end over the Phase 6 media runtime while maintaining the intended UI/application/backend ownership boundaries. The results are observations of the tested environment and revision, not guarantees for other SIP servers, codecs, devices, NAT topologies, or future revisions.

A transient `PJSIP_ETPNOTSUITABLE` message was observed during one outgoing INVITE, after which PJSIP retried/fell back and the call connected successfully. Existing JACK/XKB/QML import warnings also remained non-blocking in the accepted builds.

## Security and redaction

No SIP credential values, Digest authorization material, SDP/SRTP keys, complete raw SIP/PJSIP transcripts, raw audio, or packet captures are stored here. Lab extension numbers and sanitized protocol/result summaries are retained only as needed to explain the verification. Future evidence must be reviewed before staging.

## Reproduction

From the repository root, build and test both GUI modes and run the inherited media gate:

```bash
cmake --preset qt-gui-debug
cmake --build --preset qt-gui-debug --clean-first -j2
ctest --preset qt-gui-debug \
    -R 'CallManagerTest|QtRegistrationBackendTest|QtGuiShellSmokeTest' \
    --output-on-failure

export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake --preset qt-pjsip-gui-debug
cmake --build --preset qt-pjsip-gui-debug --clean-first -j2
ctest --preset qt-pjsip-gui-debug \
    -R 'CallManagerTest|QtRegistrationBackendTest|QtGuiShellSmokeTest' \
    --output-on-failure

./scripts/phase6/verify.sh
./scripts/quality/verify-docs-structure.sh
./scripts/quality/verify-evidence-structure.sh
```

For the live gate, securely register the workstation account, exercise one incoming and one outgoing call, verify two-way audio, duration and teardown/reuse behavior, and observe the `Live RTP / Audio` panel updating and clearing. Do not archive unredacted runtime logs.

## References

- `docs/phase7/README.md`
- `docs/architecture/qt-qml.md`
- `docs/interview/phase7-questions.md`
- `docs/architecture/audio-rtp.md`
- `tests/qt/test_qt_registration_backend.cpp`

## Post-closure verification update — 2026-10-01

`PHASE7_POST_CLOSURE_UPDATE_2026-10-01`

Two issues discovered during final manual verification were resolved and
re-verified after the original Phase 7 closure commit:

- **Incoming Decline:** Qt 1002 already generated `603 Decline`. The lab
  Asterisk dialplan was updated to preserve that response on the caller leg
  using `PJSIPHangup(603)`, so Redmi 1001 now receives `603 Decline` rather
  than the previous `403 Forbidden` mapping.
- **Outgoing remote rejection:** when Qt 1002 calls Redmi 1001 and the remote
  side rejects after Ringing but before Answer, Qt now treats the disconnect
  as a normal call end and returns to **Ready to call** instead of presenting
  `Request failed / Clear error`.

The outgoing-rejection correction is covered by
`QtRegistrationBackendTest.RemoteRejectAfterRingingIsNotReportedAsFailure`.
The rebuilt test suite passed `135/135`.

See `sip-decline.txt`, `manual-verification.txt`, and
`automated-verification.txt` for the post-closure evidence.
