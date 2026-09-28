# Phase 4 Evidence Scenario — SIP Registration

## Purpose

Record sanitized evidence that the SIP endpoints register successfully through the Phase 4 Asterisk PJSIP configuration.

## Preconditions

Asterisk is running with the Phase 4 endpoint/AoR/auth configuration, the client endpoint is reachable on the LAN, and runtime credentials are supplied outside version control.

## Procedure

Start Asterisk and the SIP endpoint, perform registration, and inspect the endpoint/contact state using the Phase 4 verification procedure. Record only the state needed to prove successful registration.

## Observations

The completed Phase 4 lab observed successful PJSIP registration and a reachable contact. Contact ports may be ephemeral and are not stable identifiers.

## Result

Registration verification: PASS.

## Artifacts

Retain only sanitized registration summaries or command output that does not expose SIP credentials or live authorization material.

## Security and redaction

Do not retain SIP passwords, Digest Authorization response material, raw SIP logger output, or packet captures containing credentials.

## References

- `evidence/phase4/README.md`
- `docs/phase4/registration.md`
- `docs/protocols/sip.md`
