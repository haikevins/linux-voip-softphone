# Phase 4 Evidence Scenario — SIP Call Flow

## Purpose

Record sanitized evidence that the Phase 4 endpoints establish and tear down calls through Asterisk with the expected SIP/SDP behavior.

## Preconditions

Both endpoints are registered, the Phase 4 dialplan is active, and runtime credentials remain outside version control.

## Procedure

Place the Phase 4 test call, observe call establishment through Asterisk, confirm negotiated media information needed by the phase, and hang up cleanly. Use the documented manual verification procedure rather than storing raw secret-bearing SIP traffic.

## Observations

The completed lab observed successful call establishment and teardown through Asterisk. Asterisk acts as the B2BUA, so the call is represented by two signaling legs. PCMU/8000 media negotiation was observed during the validated flow.

## Result

Call-flow verification: PASS.

## Artifacts

Retain only sanitized call summaries and observations needed to support the result. The retained call-flow files are indexed in `evidence/phase4/README.md` and stored in this scenario directory; obsolete historical evidence paths are not used.

## Security and redaction

Do not retain raw SIP logger output containing Digest Authorization or SRTP key material. Do not commit packet captures merely to demonstrate the flow.

## References

- `evidence/phase4/README.md`
- `docs/phase4/call-flow.md`
- `docs/protocols/sip.md`
