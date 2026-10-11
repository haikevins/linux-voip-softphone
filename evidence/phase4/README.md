# Phase 4 Evidence — SIP + Asterisk

## Status

**Completed** — Phase 4 milestone tag `v0.5-sip-asterisk`.

## Verification scope

Evidence covers Asterisk PJSIP registration, SIP call establishment/teardown, SDP/media-negotiation observations, and the expected two-leg B2BUA behavior of the Phase 4 lab.

## Environment

The accepted lab used Asterisk 20 on Ubuntu with a second SIP endpoint on the LAN. Runtime addresses, contact ports, and similar transport details are observations rather than stable configuration contracts.

## Automated verification

Phase 4 is primarily a live integration lab. Repository build/tests remain relevant, while registration and call-flow verification require live Asterisk endpoints.

## Manual verification

The retained scenario evidence records successful endpoint registration and successful call establishment/teardown through Asterisk.

## Artifacts

Registration scenario:

- `registration/README.md` — registration scenario procedure and observed result.
- `registration/asterisk-version.txt` — Asterisk runtime version.
- `registration/endpoints.txt` — sanitized PJSIP endpoint state.
- `registration/aors.txt` — observed address-of-record state.
- `registration/contacts.txt` — registered contact state.
- `registration/registration-summary.txt` — registration outcome summary.

Call-flow scenario:

- `call-flow/README.md` — call-flow scenario procedure and observed result.
- `call-flow/call-summary.txt` — established-call and teardown summary.
- `call-flow/channels-verbose.txt` — sanitized active-channel details.
- `call-flow/contacts.txt` — contacts relevant to the call scenario.
- `call-flow/pjsip-channels.txt` — PJSIP channel state during the call.

The two scenario READMEs use the scenario-evidence schema defined in `evidence/README.md`.

## Results

Within the recorded verification scope, phase 4 registration and call-flow verification passed. Observed media negotiation belongs to this SIP lab; application-controlled audible media is a later-phase concern.

## Security and redaction

Never commit SIP passwords, live Digest Authorization material, raw secret-bearing SIP logger output, SRTP inline key material, or packet captures containing credentials.

## Reproduction

Follow `docs/phase4/README.md` and the procedures in the two scenario READMEs. Retain only sanitized outcomes needed to support the result.

## References

- `docs/phase4/README.md`
- `docs/architecture/sip-asterisk.md`
- `docs/protocols/sip.md`
- `docs/interview/phase4-questions.md`
- Git tag `v0.5-sip-asterisk`
