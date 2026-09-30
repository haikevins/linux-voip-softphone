# Phase 6 Evidence — RTP and audio

## Status

Verification outcomes recorded for Phase 6. Implementation, audio/RTP diagnostics, automated checks, and outgoing/incoming audible two-way calls passed in the local lab. Sanitized evidence is assembled here; milestone release review and tagging remain pending.

## Verification scope

Real Linux audio capture/playback with stable ALSA device selection, PJSUA2 bidirectional call-media routing, PCMU media negotiation, live RTP/RTCP statistics, signaling and clean teardown, full regression/quality checks, and operator-confirmed two-way listening. The recorded values are observations from specific local runs, not network-quality guarantees.

## Environment

Ubuntu workstation with PJPROJECT/PJSIP 2.17, the existing local Asterisk lab, and a phone SIP client. Workstation account 1002 and phone account 1001 were the test endpoints. The workstation used the `default` capture/playback selector with the `ALSA` driver and an operator-selected headset. Credentials were supplied through a runtime environment variable; values are not retained.

## Automated verification

`./scripts/phase6/verify.sh` passed on 2026-09-30. The wrapper exercised PJSUA2 audio preflight, inherited Phase 5 build/quality/regression gates, plus focused RTP and PJSUA2 diagnostics suites. The document-only Commit 9 verification also passed the documentation structure check. See `automated-verification.txt` for observed test counts and the revisions involved.

## Manual verification

On 2026-09-30, the operator tested outgoing 1002-to-1001 and incoming 1001-to-1002 calls against local Asterisk using the Commit 7 media implementation (`273ad69`). Both reached SIP 200/CONFIRMED with `audio=yes`, PCMU at 8 kHz, nonzero bidirectional RTP counters, and clean disconnect. The operator separately confirmed that actual audio was audible in both directions during each call. See `live-registration.txt`, `live-outgoing-call.txt`, and `live-incoming-call.txt`.

## Artifacts

- `automated-verification.txt`: exact observed gate results and test counts.
- `live-registration.txt`: registration observed during both live call programs.
- `live-outgoing-call.txt`: outgoing call/media counters and listening outcome.
- `live-incoming-call.txt`: incoming call/media counters and listening outcome.
- `phase6-summary.txt`: concise acceptance record and revision boundaries.

Only curated summaries are retained; full terminal transcripts and captures are deliberately excluded.

## Results

The recorded automated and manual acceptance gates passed for the stated local configuration. RTP counters corroborate packet flow; the separate operator listening confirmation supports the audible-media result. These observations do not claim that every audio device, network, or future revision has been tested. Phase 6 release review/tagging are not part of this evidence commit.

## Security and redaction

No SIP credential values, Digest response headers, SDP/SRTP keys, raw SIP/PJSIP transcripts, peer contact details beyond lab extension numbers, or packet captures are retained. Runtime-only credentials are never written into evidence. Review any future added logs before staging.

## Reproduction

From the repository root, expose the existing PJPROJECT 2.17 pkg-config path and run:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase6/verify.sh
./scripts/quality/verify-docs-structure.sh
./scripts/quality/verify-evidence-structure.sh
```

For the separate live gate, follow `docs/phase6/README.md`: securely supply the runtime SIP credential, select the actual capture/playback devices, make outgoing and incoming calls, verify audible speech in both directions, and inspect `[MEDIA]` and `[RTP]` before hangup. Never archive unredacted console output.

## References

- `docs/phase6/README.md`
- `docs/architecture/audio-rtp.md`
- `scripts/phase6/verify.sh`
- `scripts/phase6/verify-audio-environment.sh`
- `evidence/phase5/README.md`
