# Phase 4 Evidence — SIP + Asterisk

## Status

Complete. Phase 4 established the SIP/Asterisk lab and the `v0.5-sip-asterisk` milestone.

## Verification scope

Evidence demonstrates Asterisk PJSIP endpoint registration, SIP call establishment and teardown, SDP/media negotiation observations, and the expected two-leg B2BUA behavior of the lab.

## Environment

The completed lab used Asterisk 20 on the Ubuntu host with a second SIP endpoint on the LAN. Runtime addresses and ephemeral contact ports are observations, not stable configuration contracts.

## Automated verification

Phase 4 is primarily a live integration lab rather than a self-contained automated SIP-stack implementation. Repository build/tests remain relevant, while registration and call-flow verification require live Asterisk endpoints.

## Manual verification

The retained Phase 4 evidence records successful endpoint registration and successful call establishment/teardown through Asterisk. Registration and call-flow evidence are indexed separately under this phase.

## Artifacts

Active evidence scenarios:

- `registration/` — sanitized registration observations.
- `call-flow/` — sanitized call-flow and media-negotiation observations.

Historical `protocol/` and `final/` evidence material from the former layout is preserved under `artifacts/` so it remains available without acting as a parallel documentation taxonomy.

## Results

The Phase 4 SIP/Asterisk lab was verified successfully, including registration and call-flow behavior. Observed media negotiation belongs to Phase 4 evidence; application-controlled audible media is a later phase concern.

## Security and redaction

Never commit SIP account passwords, live Digest Authorization material, raw secret-bearing SIP logger output, SRTP inline key material, or packet captures containing credentials.

## Reproduction

Follow `docs/phase4/README.md` and the scenario procedures under this evidence directory. Record only sanitized outcomes needed to support the result.

## References

- `docs/phase4/README.md`
- `docs/architecture/sip-asterisk.md`
- `docs/protocols/sip.md`
- `docs/interview/phase4-questions.md`
- Git tag `v0.5-sip-asterisk`
