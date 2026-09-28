# SIP Protocol — Signaling and Session Control

## Status

Established in Phase 4 and reused by the PJSIP client work in Phase 5. This document is the protocol-level reference for the SIP behavior observed in the lab; system architecture and phase execution procedures live elsewhere.

## Purpose

Describe the SIP wire/session semantics needed by this project without mixing them with Asterisk component architecture, phase-specific setup steps, or raw evidence.

The protocol document answers questions such as:

- how registration establishes a reachable contact;
- how an INVITE transaction becomes an established dialog;
- how a B2BUA produces two independent SIP legs;
- how SDP relates signaling to the media path;
- how normal challenge/error responses must be interpreted in transaction context;
- how an established dialog is torn down.

## Scope

Included:

- REGISTER challenge/response behavior;
- INVITE authentication and call establishment;
- provisional and final SIP responses used by the Phase 4 lab;
- Asterisk B2BUA dialog behavior as observed on the wire;
- SDP offer/answer relationship to media negotiation;
- BYE-based teardown;
- the observed secure-media rejection followed by a compatible retry;
- security rules for protocol evidence.

Excluded:

- Asterisk object/component design, which belongs in `docs/architecture/sip-asterisk.md`;
- lab setup and execution procedures, which belong in `docs/phase4/`;
- application-owned PJSUA2 lifecycle design, which belongs in `docs/architecture/pjsip-cli.md`;
- RTP packet timing, sequence, jitter, and audio-device behavior, which are Phase 6 concerns.

## Roles and terminology

The Phase 4 lab uses these protocol roles:

- **user agent (UA)** — a SIP endpoint that registers and participates in calls;
- **registrar** — the server role that accepts REGISTER transactions and maintains reachable contacts;
- **AoR** — the stable address-of-record associated with an identity;
- **contact** — the current reachable URI learned through registration;
- **transaction** — one SIP request and its responses;
- **dialog** — the longer-lived SIP relationship created by dialog-forming requests such as INVITE;
- **B2BUA** — a back-to-back user agent that terminates one dialog and creates another instead of forwarding one unchanged end-to-end dialog;
- **SDP** — the media-description format carried by SIP messages for offer/answer negotiation;
- **RTP** — the media transport used after compatible media parameters have been negotiated.

In this project, Asterisk acts as registrar and B2BUA for the two lab endpoints.

## Wire model

### Registration

A typical successful registration is:

```text
User agent                    Asterisk
    |                            |
    | REGISTER                   |
    |--------------------------->|
    |                            |
    | 401 Unauthorized           |
    |<---------------------------|
    |                            |
    | REGISTER + Digest response |
    |--------------------------->|
    |                            |
    | 200 OK                     |
    |<---------------------------|
```

The first `401 Unauthorized` is the Digest challenge. The successful result is the authenticated follow-up transaction plus a live contact binding.

### Authenticated call establishment through a B2BUA

The caller leg may also be challenged before the authenticated INVITE proceeds. After the caller leg is accepted, Asterisk creates an independent callee leg:

```text
1001                       Asterisk                       1002
 |                            |                            |
 | authenticated INVITE       |                            |
 |--------------------------->|                            |
 | 100 Trying                 |                            |
 |<---------------------------|                            |
 |                            | INVITE                     |
 |                            |--------------------------->|
 |                            | 180 Ringing                |
 |                            |<---------------------------|
 | 180 Ringing                |                            |
 |<---------------------------|                            |
 |                            | 200 OK                     |
 |                            |<---------------------------|
 |                            | ACK                        |
 |                            |--------------------------->|
 | 200 OK                     |                            |
 |<---------------------------|                            |
 | ACK                        |                            |
 |--------------------------->|                            |
```

Because Asterisk terminates and recreates signaling, the two legs can have independent Call-ID values, tags, CSeq values, addresses, ports, and other dialog state.

### Established-dialog teardown

The verified call was terminated after the dialog was established, so teardown used BYE rather than CANCEL:

```text
1001                       Asterisk                       1002
 |                            |                            |
 |                            | BYE                        |
 |                            |<---------------------------|
 |                            | 200 OK                     |
 |                            |--------------------------->|
 | BYE                        |                            |
 |<---------------------------|                            |
 | 200 OK                     |                            |
 |--------------------------->|                            |
```

## Runtime behavior

Registration and call-dialog lifetimes are separate. A contact can remain registered before, during, and after a call. Ending the call removes the active call dialogs/channels but does not inherently remove the registration.

For an INVITE transaction in this lab:

- `100 Trying` indicates that request processing has started;
- `180 Ringing` indicates that the destination is alerting;
- `200 OK` is the successful final response and carries the answer-side media description when applicable;
- ACK completes the successful INVITE exchange and the dialog becomes established;
- BYE terminates an established dialog;
- CANCEL is used for a still-pending INVITE rather than an already established dialog.

## Interactions and dependencies

SIP controls the session, but it does not carry the real-time audio stream. SDP is carried inside relevant SIP messages and describes media parameters such as media profile, codec, payload mapping, addresses, and ports. RTP then carries media using the negotiated parameters.

The Phase 4 architecture deliberately uses Asterisk with `direct_media=no`, so the observed media topology is conceptually:

```text
1001 RTP <----> Asterisk RTP <----> 1002 RTP
```

The architecture reason for that topology belongs in `docs/architecture/sip-asterisk.md`; this document records only the protocol relationship between SIP/SDP negotiation and the resulting media session.

## Failure modes

Two observed responses require transaction-level interpretation:

- `401 Unauthorized` can be an expected Digest challenge. It is not a failure if the authenticated retry succeeds and the transaction ends successfully.
- `488 Not Acceptable Here` was observed for an offered media profile that the current lab configuration could not negotiate. The client retried with a compatible RTP profile and the later call attempt succeeded.

Therefore:

```text
one rejected transaction or offer != final user-visible call failure
```

Correct diagnosis requires correlating each response with the request, authentication attempt, and SDP offer to which it belongs.

SIP signaling success also does not by itself prove correct audio. A dialog may establish while media still fails because of routing, codec, RTP, firewall, or device-path problems.

## Security considerations

Raw SIP logger output is not a repository artifact. Authentication-bearing messages can contain dynamic Digest authorization values, and SDP can contain security/keying attributes depending on the negotiated media profile.

Repository documentation and evidence therefore follow these rules:

- never store SIP passwords in documentation or evidence;
- do not commit raw authentication-bearing SIP traces;
- do not commit media keying material;
- prefer sanitized protocol summaries and selected non-secret runtime state;
- treat addresses and ephemeral ports as runtime observations rather than stable secrets or constants.

## Verification observations

The completed Phase 4 lab established the following observable protocol facts:

- both identities obtained live registered contacts;
- authenticated SIP signaling reached a successful call;
- Asterisk exposed two independent PJSIP call legs while acting as a B2BUA;
- ringing and answer propagation completed with `180 Ringing`, `200 OK`, and ACK;
- an incompatible secure-media offer was rejected, followed by a compatible retry;
- the successful negotiation used `PCMU/8000`;
- BYE/`200 OK` teardown returned Asterisk to zero active calls while registrations remained available;
- committed evidence contains sanitized summaries rather than raw SIP logger output.

These observations are evidence of the Phase 4 lab behavior, not a claim that every SIP deployment uses exactly the same authentication, routing, media, or retry policy.

## Phase boundary

Phase 4 establishes SIP signaling, SDP negotiation, B2BUA behavior, and sanitized protocol evidence with external clients. Phase 5 reuses the same SIP behavior through project-owned PJSUA2 clients. Phase 6 moves deeper into RTP/audio transport and application media routing.

This document remains the SIP protocol reference across those phases. Phase-specific implementation and verification details stay in the corresponding `docs/phaseN/` and `docs/architecture/` documents.

## References

- `docs/architecture/sip-asterisk.md`
- `docs/phase4/README.md`
- `docs/phase4/registration.md`
- `docs/phase4/call-flow.md`
- `docs/interview/phase4-questions.md`
- `evidence/phase4/`
