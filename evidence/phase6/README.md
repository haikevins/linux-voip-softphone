# Phase 6 Evidence — RTP and Audio

## Status

**Completed** — Phase 6 milestone tag `v0.7-rtp-audio`. Accepted for the documented local lab.

## Verification scope

Evidence covers Linux capture/playback selection, bidirectional PJSUA2 media routing, PCMU negotiation, RTP/RTCP diagnostics, signaling/teardown, automated regressions, operator-confirmed two-way audio, and one controlled approximately 75-second Wireshark RTP study.

## Environment

The accepted lab used Ubuntu, PJPROJECT/PJSIP 2.17, local Asterisk, a phone SIP client, and operator-selected Linux audio devices. Credentials were supplied only at runtime and are not retained.

## Automated verification

Run:

```bash
./scripts/phase6/verify.sh
```

The retained automated record contains the observed test counts and revision boundaries for the accepted Phase 6 gate.

## Manual verification

Outgoing and incoming live calls both reached confirmed media state with PCMU, nonzero bidirectional RTP counters, clean teardown, and operator-confirmed intelligible audio in both directions.

A separate approximately 75-second outgoing call was inspected with Wireshark. The retained summary describes the selected opposite-direction RTP streams and the intentional silence interval; the raw capture remains private.

## Artifacts

- `automated-verification.txt` — automated gate results.
- `live-registration.txt` — registration observation.
- `live-outgoing-call.txt` — outgoing media/RTP and listening result.
- `live-incoming-call.txt` — incoming media/RTP and listening result.
- `phase6-summary.txt` — concise acceptance/revision summary.
- `rtp-wireshark.txt` — sanitized wire-level RTP study.

## Results

The recorded automated and manual gates passed for the stated local configuration. RTP counters and the selected Wireshark streams corroborate packet flow, while human listening establishes the audible-media result for those calls. These observations are not network-wide or future-environment guarantees.

## Security and redaction

Do not retain SIP credentials, Digest material, SDP/SRTP keys, raw SIP/PJSIP transcripts, raw audio, or packet captures containing sensitive media. The original Wireshark capture remains outside Git.

## Reproduction

Run `./scripts/phase6/verify.sh` for the automated gate. Follow `docs/phase6/README.md` for live calls and the separate wire-level study; record only sanitized outcomes.

## References

- `docs/phase6/README.md`
- `docs/architecture/audio-rtp.md`
- `docs/protocols/rtp.md`
- `scripts/phase6/verify.sh`
- `scripts/phase6/verify-audio-environment.sh`
- `evidence/phase5/README.md`
