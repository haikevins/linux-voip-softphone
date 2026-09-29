# Phase 5 Evidence — PJSIP CLI Softphone

## Status

Complete. Phase 5 established the PJSUA2/PJSIP CLI softphone and the `v0.6-pjsip-cli` milestone.

## Verification scope

Evidence covers PJSIP dependency integration, endpoint/account configuration, registration, outgoing and incoming call lifecycle, automated PJSIP tests, full regression testing, sanitizers, clang-tidy, and live Asterisk interoperability.

## Environment

The completed Phase 5 environment uses PJPROJECT/PJSIP 2.17 and Asterisk 20 on Ubuntu. Exact runtime endpoint addresses are not part of the stable evidence contract.

## Automated verification

The Phase 5 verifier is:

```bash
./scripts/phase5/verify.sh
```

The final automated gates passed: 33/33 PJSIP-focused tests, 93/93 full tests, 93/93 sanitizer tests, 64/64 clang-tidy build/check targets, and the 60/60 non-PJSIP regression subset.

## Manual verification

Live Asterisk registration plus outgoing and incoming call-flow checks were verified manually. Phase 5 intentionally used null audio for the CLI softphone, so negotiated RTP/media statistics do not by themselves claim audible two-way application audio.

## Artifacts

Existing committed Phase 5 evidence is retained as the historical record of the completed verification run. New artifacts must remain sanitized and should avoid raw SIP traffic.

## Results

Phase 5 automated verification passed, and live registration/outgoing/incoming call interoperability was verified. RTP/audio implementation remains the Phase 6 boundary.

## Security and redaction

SIP passwords are supplied at runtime and must not appear in evidence. Do not commit Digest Authorization headers, raw SIP logger output, SRTP inline key material, or secret-bearing packet captures.

## Reproduction

Automated gate:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase5/verify.sh
```

Live Asterisk checks remain manual and must use runtime credentials without recording secrets.

## References

- `docs/phase5/README.md`
- `docs/phase5/dependencies.md`
- `docs/architecture/pjsip-cli.md`
- `docs/interview/phase5-questions.md`
- Git tag `v0.6-pjsip-cli`
