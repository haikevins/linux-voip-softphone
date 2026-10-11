# Phase 5 Evidence — PJSIP CLI Softphone

## Status

**Completed** — Phase 5 milestone tag `v0.6-pjsip-cli`.

## Verification scope

Evidence covers PJSIP dependency integration, endpoint/account configuration, registration, outgoing/incoming call lifecycle, focused PJSIP tests, full regressions, sanitizers, clang-tidy, and live Asterisk interoperability.

## Environment

The accepted environment used PJPROJECT/PJSIP 2.17 and Asterisk 20 on Ubuntu. Exact runtime endpoint addresses are not part of the stable evidence contract.

## Automated verification

Run:

```bash
./scripts/phase5/verify.sh
```

The accepted gates recorded 33/33 PJSIP-focused tests, 93/93 full tests, 93/93 sanitizer tests, 64/64 clang-tidy build/check targets, and the 60/60 non-PJSIP regression subset.

## Manual verification

Live Asterisk registration plus outgoing and incoming call-flow checks passed. Phase 5 intentionally used null audio, so negotiated media/RTP state does not claim audible two-way application audio.

## Artifacts

- `automated-verification.txt` — automated gate results.
- `live-registration.txt` — sanitized registration observation.
- `live-outgoing-call.txt` — sanitized outgoing-call observation.
- `live-incoming-call.txt` — sanitized incoming-call observation.
- `phase5-summary.txt` — concise milestone summary.

## Results

Within the recorded verification scope, automated verification and live registration/outgoing/incoming interoperability passed. RTP/audio implementation remains the Phase 6 boundary.

## Security and redaction

SIP passwords are runtime-only. Do not commit Digest Authorization headers, raw SIP logger output, SRTP inline key material, or secret-bearing packet captures.

## Reproduction

Expose the local PJPROJECT 2.17 pkg-config path and run `./scripts/phase5/verify.sh`. Live Asterisk checks remain manual and must use runtime credentials without recording secrets.

## References

- `docs/phase5/README.md`
- `docs/phase5/dependencies.md`
- `docs/architecture/pjsip-cli.md`
- `docs/interview/phase5-questions.md`
- Git tag `v0.6-pjsip-cli`
