# Phase 5 PJSIP CLI evidence

This directory records the final, sanitized verification evidence for Phase 5.

Phase 5 adds PJPROJECT/PJSUA2 integration and three command-line flows:

- `voip-sip-register` for one-shot SIP registration verification;
- `voip-sip-call` for an outgoing SIP call lifecycle;
- `voip-sip-listen` for an incoming SIP call lifecycle.

## Evidence files

- `automated-verification.txt` records the final automated quality-gate results.
- `live-registration.txt` records the sanitized registration result against the Asterisk lab.
- `live-outgoing-call.txt` records the sanitized outgoing-call result.
- `live-incoming-call.txt` records the sanitized incoming-call result.
- `phase5-summary.txt` records the Phase 5 commit chain, acceptance result, and scope boundary.

## Evidence policy

These artifacts intentionally contain summaries rather than raw SIP packet/logger captures.

Do not commit any of the following:

- SIP `Authorization` or `Proxy-Authorization` headers;
- Digest response values, nonces, opaque values, or plaintext account passwords;
- raw SDP media-keying attributes or inline keying material;
- environment dumps that could reveal `VOIP_SIP_PASSWORD` or other credentials.

The environment-variable name `VOIP_SIP_PASSWORD` may be documented, but its value must never be
stored in the repository.

The RTP statistics below demonstrate media negotiation and packet flow only. Phase 5 does not claim
application-level audible audio; audio-device routing and media handling remain Phase 6 scope.
