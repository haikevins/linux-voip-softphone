# Phase 4 — SIP + Asterisk Lab

## Status

**Completed** — milestone tag `v0.5-sip-asterisk`.

Phase 4 proves the SIP/Asterisk lab independently of the custom C++ client and documents the signaling, SDP, call, and evidence model used by Phase 5.

## Objective

Build a known-good SIP environment with Asterisk and two external SIP endpoints, verify registration and a complete call lifecycle, and explain the observed protocol behavior before integrating PJSIP into the project.

## Scope

Included in this phase:

- local Asterisk PJSIP transport, endpoint, auth, AoR, and dialplan configuration;
- two SIP identities (`1001` and `1002`);
- SIP registration verification;
- live call verification through Asterisk;
- SIP digest challenge analysis;
- INVITE/ringing/answer/ACK/BYE analysis;
- SDP/media-negotiation observations;
- sanitized Asterisk runtime evidence;
- explicit secret-handling rules.

Deliberately excluded:

- custom PJSIP/PJSUA2 C++ clients;
- production credential storage;
- application-owned RTP/audio routing;
- Qt/QML UI.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| Asterisk PJSIP sample | Define lab transport/endpoints/auth/AoRs | `config/asterisk/pjsip.conf.example` |
| Dialplan sample | Route calls between the lab endpoints | `config/asterisk/extensions.conf.example` |
| Asterisk verification | Validate sample/runtime configuration | `scripts/phase4/verify-asterisk.sh` |
| Registration verification | Verify live contacts | `scripts/phase4/verify-registration.sh` |
| Call verification | Verify the established-call runtime state | `scripts/phase4/verify-call.sh` |
| Final verification | Close the phase against current/evidence state | `scripts/phase4/verify.sh` |
| Architecture note | Explain Asterisk/B2BUA and signaling/media paths | `docs/architecture/sip-asterisk.md` |
| Protocol reference | Explain observed SIP wire/session behavior | `docs/protocols/sip.md` |
| Interview review | Capture Phase 4 review questions | `docs/interview/phase4-questions.md` |

## Architecture and design

The final Phase 4 lab topology is:

```text
Redmi / Linphone / 1001
          |
          | SIP + media
          v
Asterisk on Ubuntu
PJSIP UDP :5060
B2BUA, direct_media=no
          |
          | SIP + media
          v
ThinkPad SIP client / 1002
```

Asterisk is a back-to-back user agent. It terminates one SIP dialog and creates another dialog toward the destination contact; it is not acting as a transparent proxy.

With `direct_media=no`, Asterisk remains in the media path. This makes the two call legs and media negotiation easier to inspect during the lab.

The observed successful call included:

```text
REGISTER challenge/response -> 200 OK
INVITE authentication challenge -> authenticated INVITE
100 Trying
180 Ringing
200 OK
ACK
media negotiated
BYE
200 OK
```

One client initially offered a secure media profile that the current lab configuration could not accept, producing `488 Not Acceptable Here`; the client then retried with a compatible RTP offer and the successful call negotiated `PCMU/8000`. The rejected offer and the final successful dialog must not be confused.

## Build and run

Validate the checked-in Asterisk samples:

```bash
./scripts/phase4/verify-asterisk.sh --samples
```

After installing the lab configuration into Asterisk, verify the runtime:

```bash
sudo ./scripts/phase4/verify-asterisk.sh --runtime
```

Useful runtime inspection commands include:

```bash
sudo asterisk -rx "pjsip show endpoints"
sudo asterisk -rx "pjsip show aors"
sudo asterisk -rx "pjsip show contacts"
sudo asterisk -rx "dialplan show voip-lab"
```

Registration and call experiments are documented separately so the phase README remains a high-level entry point.

## Verification

Run the progressively stronger checks:

```bash
sudo ./scripts/phase4/verify-asterisk.sh --runtime
sudo ./scripts/phase4/verify-registration.sh --check
sudo ./scripts/phase4/verify-call.sh --check
sudo ./scripts/phase4/verify.sh --check
```

The final verification confirms the live registration state, idle Asterisk state after teardown, previously captured established-call evidence, and evidence-safety rules. `scripts/phase4/verify.sh` is the authoritative final verifier; protocol explanation belongs under `docs/protocols/`, while concrete runtime observations belong under `evidence/phase4/`.

Evidence capture is scenario-specific rather than aggregate. Capture registration state while the endpoints are registered, and capture call-flow state while the call is established:

```bash
sudo ./scripts/phase4/verify-registration.sh --capture
sudo ./scripts/phase4/verify-call.sh --capture
```

## Evidence

Committed evidence lives under:

```text
evidence/phase4/
```

The repository stores selected non-secret Asterisk state and sanitized summaries. Raw SIP logger output is intentionally excluded because it can contain Digest authorization data and media keying attributes.

## Interview focus

Use `docs/interview/phase4-questions.md` together with the architecture/protocol notes. Be ready to explain:

- endpoint, auth, AoR, contact, transport, and dialplan roles;
- why a `401 Unauthorized` challenge can be normal SIP Digest behavior;
- Asterisk as a B2BUA versus a transparent proxy;
- why `direct_media=no` keeps Asterisk in the media path;
- INVITE, `100 Trying`, `180 Ringing`, `200 OK`, ACK, BYE, and CANCEL roles;
- SDP negotiation and why an incompatible offer can produce `488`;
- the difference between SIP signaling and RTP media;
- why raw authentication-bearing traces are not committed.

## Exit criteria

Phase 4 is complete when all of the following statements are true:

- both SIP identities can register to the local Asterisk server;
- the call between the two endpoints can be established and torn down cleanly;
- Asterisk shows the expected two-leg B2BUA behavior during the call;
- the successful media negotiation is documented from observed behavior;
- final Asterisk/SIP verification passes;
- protocol and architecture documentation are present;
- evidence is sanitized and committed;
- milestone tag `v0.5-sip-asterisk` identifies the completed phase.

## References

- `docs/architecture/sip-asterisk.md` — Architecture
- `docs/phase4/registration.md` — Registration experiment
- `docs/phase4/call-flow.md` — Call-flow experiment
- `docs/protocols/sip.md` — SIP protocol reference
- `docs/README.md` — Documentation taxonomy
- `docs/interview/phase4-questions.md` — Interview review
- `evidence/phase4/` — Evidence

## Next phase

Phase 5 replaces the temporary desktop SIP endpoint with project-owned PJSUA2 command-line clients while reusing this known-good Asterisk lab.
