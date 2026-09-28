# SIP + Asterisk Architecture — Phase 4

## Purpose

Phase 4 establishes a known-good SIP lab before the project introduces its own PJSIP client. The architecture is intentionally simple: Asterisk is the central SIP server/B2BUA, two external user agents register to it, and the dialplan routes calls between the registered contacts.

The phase exists to separate two classes of problem:

```text
SIP/Asterisk lab correctness
        versus
custom C++/PJSIP client correctness
```

If registration and calling are already proven with external clients, later PJSIP failures can be investigated without first questioning the server, dialplan, or LAN topology.

## Topology

The final verified Phase 4 topology is:

```text
Redmi 12 5G
Linphone / endpoint 1001
        |
        | SIP signaling
        | RTP media
        v
+------------------------------+
| ThinkPad / Ubuntu            |
|                              |
| Asterisk                     |
| PJSIP UDP :5060              |
| voip-lab dialplan context    |
| direct_media=no              |
+------------------------------+
        |
        | SIP signaling
        | RTP media
        v
ThinkPad SIP client
endpoint 1002
```

The specific client application used for an endpoint is a lab detail. The architectural contract is that both endpoints register to the same Asterisk instance and calls are routed through Asterisk.

## Asterisk object model

The lab configuration uses the PJSIP object model:

```text
transport
  |
  +--> UDP listener on Asterisk

endpoint 1001 ---- auth 1001 ---- AoR 1001 ---- registered contact
endpoint 1002 ---- auth 1002 ---- AoR 1002 ---- registered contact

endpoint context
  |
  v
voip-lab dialplan
```

The responsibilities are distinct:

- **transport** defines how SIP messages reach Asterisk;
- **endpoint** defines signaling/media policy for a SIP identity;
- **auth** defines authentication material associated with the identity;
- **AoR** represents the address-of-record and contains live registered contacts;
- **contact** is the current reachable user-agent URI learned through registration;
- **dialplan context** decides how a dialed extension is routed.

The repository examples use one endpoint/auth/AoR set for `1001` and another for `1002`.

## Registration path

A typical registration flow in the lab is:

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

The initial `401 Unauthorized` is a challenge in SIP Digest authentication. It is not, by itself, evidence that registration failed. The relevant result is the authenticated follow-up transaction and the final registered contact state.

## Call path and B2BUA behavior

Asterisk behaves as a back-to-back user agent rather than a transparent SIP proxy. The caller dialog terminates at Asterisk, and Asterisk creates a second dialog toward the callee.

Conceptually:

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

Because the two dialogs are independent, identifiers such as Call-ID, tags, sequence numbers, local ports, and other transaction/dialog state do not have to be identical on the two sides.

## Dialplan routing

Both endpoints use the `voip-lab` context. The dialplan resolves the requested extension to the currently registered PJSIP contact rather than hard-coding a transient client address.

That matters because SIP user agents can register from changing IP addresses or source ports. The AoR/contact state is the dynamic routing information; the dialed extension is the stable application-level identity.

## Media path

The lab uses:

```text
direct_media=no
```

Therefore media remains anchored through Asterisk instead of being re-invited directly between the two user agents.

Conceptually:

```text
1001 RTP <----> Asterisk RTP <----> 1002 RTP
```

This makes the lab easier to inspect because Asterisk remains a visible participant in both signaling and media.

During the verified call, one client initially offered a secure RTP profile that the current lab configuration could not negotiate. Asterisk returned `488 Not Acceptable Here` for that offer. The client retried with a compatible RTP profile, and the successful call negotiated `PCMU/8000`.

The important distinction is:

```text
failed media offer != failed final call
```

A response from an earlier negotiation attempt must be interpreted in the context of the complete SIP dialog sequence.

## Teardown

A successful teardown uses BYE on the established dialogs:

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

After teardown, the call channels disappear while the SIP registrations can remain active. Registration lifetime and call-dialog lifetime are separate concerns.

## Verification model

Phase 4 uses progressively stronger verification:

```text
Asterisk sample configuration
        |
        v
Asterisk runtime objects
        |
        v
registered contacts
        |
        v
active call state
        |
        v
sanitized protocol summary
        |
        v
final idle-state verification
```

The repository scripts are:

```text
scripts/verify_asterisk.sh
scripts/verify_sip_registration.sh
scripts/verify_sip_call.sh
scripts/verify_phase4.sh
```

## Evidence and security boundary

Raw SIP logger output is intentionally not committed. SIP traces can include Digest authorization fields and media keying attributes that should not be stored in the repository.

Committed evidence is restricted to selected non-secret runtime state and sanitized summaries under:

```text
evidence/04-sip/
```

The documentation explains protocol behavior without preserving reusable authentication material.

## Phase boundary

Phase 4 proves the environment and protocol behavior:

```text
Asterisk configuration
SIP registration
SIP call establishment
SDP/media negotiation observation
B2BUA behavior
call teardown
sanitized evidence
```

It does not implement the project's own SIP user agent. Phase 5 introduces PJSUA2 clients and reuses this lab as the integration target.
