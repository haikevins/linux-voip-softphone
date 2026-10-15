# Phase 6 Evidence — RTP and Audio

## Status

Completed. Phase 6 established RTP audio streaming and the `v0.7-rtp-audio` milestone.

## Verification scope

Evidence covers Linux capture/playback preflight, active two-way PJSUA2 media routing, PCMU negotiation, RTP/RTCP diagnostics, automated quality gates, outgoing/incoming Asterisk calls, audible speech in both directions, and one controlled Wireshark RTP study.

## Environment

The recorded lab used Ubuntu, PJPROJECT/PJSIP 2.17, Asterisk 20, workstation SIP account `1002`, and phone SIP account `1001`. Audio capture and playback were selected by `default` with the `ALSA` driver; the operator used the selected headset. Addresses, actual device mappings and RTP measurements are local observations, not portable defaults.

## Automated verification

The Phase 6 verifier is:

```bash
./scripts/phase6/verify.sh
```

The 2026-09-30 run passed audio preflight, the inherited Phase 5 checks, focused RTP/diagnostics tests, and quality gates. The recorded counts were 45/45 PJSIP-focused, 117/117 full PJSIP, 117/117 sanitizer, 72/72 non-PJSIP, 4/4 RTP statistics, and 3/3 PJSUA2 diagnostics tests, plus `clang-tidy`. These counts identify the tested revision, not a fixed future test contract. See `automated-verification.txt` for details.

## Manual verification

On 2026-09-30, outgoing (`1002` to `1001`) and incoming (`1001` to `1002`) calls both reached confirmed signaling, negotiated PCMU at 8 kHz, recorded nonzero RTP send/receive counts, and disconnected cleanly. The operator separately confirmed intelligible speech in both directions for each call.

A distinct approximately 75-second outgoing-call Wireshark study inspected two selected opposite-direction RTP streams. The operator reported a deliberate silence interval near seconds 60–75. Selected streams showed expected 160-tick timestamp steps for 20-ms PCMU packets and no sequence errors or packet loss under the recorded Wireshark analysis. Pauses in one direction are documented as observations; their cause is not conclusively established. This study does not replace the incoming-call verification.

## Artifacts

- `automated-verification.txt` — gates, observed test counts and revision context.
- `live-registration.txt` — registration results observed during the live tests.
- `live-outgoing-call.txt` — outgoing call, codec/RTP measurements and listening result.
- `live-incoming-call.txt` — incoming call, codec/RTP measurements and listening result.
- `rtp-wireshark.txt` — selected RTP header and stream statistics for the controlled study.
- `phase6-summary.txt` — consolidated acceptance record and commit boundaries.

These are curated summaries rather than complete console logs or packet captures. Media implementation, verification, documentation and evidence were recorded across commits `273ad69`, `bbaec38`, `c2354b3`, `8337297` and `4582baa`; the artifacts retain their own revision context.

## Results

The recorded automated and live-call checks passed for this workstation and lab. RTP counters and Wireshark corroborate observed packet transport; the separate listening checks support the claim of audible media. Zero loss in **two selected streams** cannot establish network-wide zero loss or guarantee device behavior in other environments.

## Security and redaction

Do not commit SIP passwords, Digest Authorization material, raw sensitive SIP/PJSIP logs, SDP/SRTP keys, identifiable peer details, or `.pcap`/`.pcapng` captures. PCMU RTP payloads may disclose speech. Supply credentials at runtime and sanitize new evidence before staging.

## Reproduction

From the repository root:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase6/verify.sh
./scripts/quality/verify-docs-structure.sh
./scripts/quality/verify-evidence-structure.sh
```

Follow `docs/phase6/README.md` for separate outgoing/incoming live calls, device selection and human listening checks. Preserve only sanitized result summaries.

## References

- `docs/phase6/README.md`
- `docs/architecture/audio-rtp.md`
- `docs/protocols/rtp.md`
- `docs/interview/phase6-questions.md`
- `scripts/phase6/verify.sh`
- Milestone identifier `v0.7-rtp-audio`
