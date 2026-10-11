# Phase 4 Interview Review — SIP + Asterisk

Use the repository implementation, documentation, tests, and evidence as the basis for the answers. Explain the engineering decision and the observed behavior; do not memorize isolated one-line definitions.

## Review scope

Base each answer on the implementation, tests, and retained evidence. Distinguish design intent, observed behavior, and unverified boundaries.

- known-good SIP lab design
- Asterisk PJSIP object model
- Digest registration challenge flow
- B2BUA and dialplan behavior
- SDP/RTP negotiation
- call teardown, evidence, and security boundaries

## Questions and answers

### Lab architecture and registration

#### Why was Asterisk validated with existing SIP clients before integrating PJSIP into C++?

It separates server/lab problems from custom-client problems. If two known SIP user agents can register and complete a call through the checked-in Asterisk configuration, the transport, endpoint objects, dialplan, LAN path, and basic media negotiation are already known to work.

When Phase 5 introduces the project-owned PJSUA2 client, a failure can then be investigated primarily as a client integration/lifecycle issue instead of simultaneously debugging the server environment.

#### What are an endpoint, auth object, AoR, and contact in the Asterisk PJSIP model?

In this lab:

- an **endpoint** describes signaling/media policy for a SIP identity;
- an **auth** object contains the authentication configuration associated with that identity;
- an **AoR** represents the address-of-record and owns the set of reachable contacts;
- a **contact** is the current registered user-agent URI learned at runtime.

The stable identity is not the same thing as a transient contact address or source port.

Repository references:

```text
config/asterisk/pjsip.conf.example
docs/architecture/sip-asterisk.md
```

#### Why can `401 Unauthorized` be normal during SIP registration?

The first unauthenticated REGISTER can be challenged by Asterisk. The client uses the challenge parameters to calculate a SIP Digest response and sends another REGISTER containing authorization data. A successful transaction ends with `200 OK` and a live contact binding.

Therefore, interpreting only the first `401` as a registration failure would be incorrect.

#### What does it mean that Asterisk is a B2BUA in this lab?

Asterisk terminates the SIP dialog from the caller and creates another SIP dialog toward the callee. It is not simply forwarding one unchanged dialog between the two user agents.

This is why the two sides can have independent Call-ID values, tags, CSeq values, addresses, and other dialog state.

Repository reference: `docs/architecture/sip-asterisk.md`.

### Call signaling and media negotiation

#### What is the role of the dialplan?

Both lab endpoints use the `voip-lab` context. When one extension calls the other, the dialplan resolves the dialed identity to the registered PJSIP contact and creates the outgoing call leg.

The dialplan is application routing logic; SIP registration provides the current contact needed to reach the endpoint.

#### Why is `direct_media=no` useful in this phase?

It keeps Asterisk in the media path instead of allowing the two endpoints to exchange media directly after signaling setup.

For the lab this is useful because:

- the signaling and media topology is easier to reason about;
- Asterisk remains a visible anchor for both call legs;
- later RTP observation can distinguish the two media legs through the server.

#### What are the roles of INVITE, `100 Trying`, `180 Ringing`, `200 OK`, and ACK?

In the successful call flow:

- INVITE starts call/session establishment and carries the media offer;
- `100 Trying` indicates that the request is being processed;
- `180 Ringing` reports that the destination is alerting;
- `200 OK` is the successful final response to the INVITE and carries the answering media description;
- ACK completes the INVITE transaction/dialog establishment after the successful final response.

The protocol-level two-leg flow is documented in `docs/protocols/sip.md`.

#### What is the difference between BYE and CANCEL?

BYE terminates an established SIP dialog. CANCEL attempts to stop a pending INVITE before that INVITE has completed with a final response.

The verified Phase 4 teardown uses BYE because the call had already reached the established state.

#### What is SDP doing in the SIP call?

SIP establishes and controls the session; SDP describes media capabilities and parameters carried inside relevant SIP messages.

The offer/answer exchange determines items such as media profile, codec, payload type, addresses, and ports. SDP negotiation is therefore related to the media path but is not itself the RTP media stream.

#### Why was `488 Not Acceptable Here` observed before the successful call?

One client first offered a secure media profile that the current lab endpoint configuration did not support. Asterisk rejected that offer with `488 Not Acceptable Here`.

The client then retried with a compatible RTP profile, and the final call succeeded with `PCMU/8000`.

The engineering lesson is to correlate a response with the specific transaction/offer instead of assuming every error response means the overall user action ultimately failed.

#### What does `PCMU/8000` mean?

`PCMU` is G.711 mu-law audio. The `8000` value is the RTP clock rate used for that codec in this negotiation.

Phase 4 observed the codec/media negotiation through the external SIP clients. Phase 5 still does not claim application-owned real audio-device handling; that is deferred to Phase 6.

#### How are SIP signaling and RTP media different?

SIP handles session signaling such as registration, invitation, ringing, answer, and teardown. RTP carries time-sensitive media packets after media parameters have been negotiated.

A call can therefore have correct SIP signaling while still having media problems such as one-way audio, an unreachable RTP address, or a device-routing issue.

### Verification and evidence security

#### Why was raw SIP logger output not committed?

Authentication-bearing SIP messages can contain dynamic Digest authorization values. Media descriptions can also contain security/keying attributes depending on the client and negotiation.

The project therefore stores sanitized summaries and selected Asterisk runtime state instead of raw logger output. This preserves engineering evidence without committing reusable secrets or keying material.

#### What does the final Phase 4 verification prove?

The final verification combines current runtime checks with previously captured sanitized evidence. It confirms that:

- Asterisk is configured and reachable;
- both endpoints have valid registered contacts;
- the established-call evidence recorded two active PJSIP legs and one call;
- the protocol summary records the expected signaling/media behavior;
- the final runtime can return to zero active channels/calls while registrations remain;
- the committed documentation/evidence excludes sensitive raw authentication or keying material.

Repository references: `docs/phase4/README.md` and `scripts/phase4/verify.sh`.

## Phase boundary

Phase 4 proves the external SIP/Asterisk environment with existing clients. The project-owned PJSUA2 client is introduced in Phase 5.

## References

- `docs/phase4/README.md`
- `docs/architecture/sip-asterisk.md`
- `docs/protocols/sip.md`
- `scripts/phase4/verify.sh`
- `evidence/phase4/`
