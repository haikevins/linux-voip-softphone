# Phase 4 — SIP Protocol Notes

## Goal

Commit 4 documents the SIP signaling observed during the successful Phase 4 call from endpoint
`1001` (Linphone Android) to endpoint `1002` (baresip) through Asterisk.

This document is intentionally based on a sanitized summary of the observed exchange. Raw PJSIP
logger output is not committed because it can include SIP digest authorization fields and media
keying attributes.

## Actors

```text
1001: Redmi 12 5G / Linphone Android
Asterisk: B2BUA on the ThinkPad
1002: baresip on the ThinkPad
```

Asterisk is a back-to-back user agent (B2BUA), not a transparent SIP proxy. It terminates the SIP
dialog from `1001` and creates a separate SIP dialog toward `1002`. Consequently, the two call legs
use independent dialog identifiers and may use different local addresses, ports, sequence numbers,
and Call-ID values.

## Caller authentication transaction

The first INVITE from `1001` is challenged by Asterisk with SIP digest authentication:

```text
1001                         Asterisk
  |                             |
  | INVITE                      |
  |---------------------------->|
  |                             |
  | 401 Unauthorized            |
  |<----------------------------|
  |                             |
  | ACK                         |
  |---------------------------->|
  |                             |
  | INVITE + Authorization      |
  |---------------------------->|
  |                             |
  | 100 Trying                  |
  |<----------------------------|
```

The initial `401 Unauthorized` is part of the expected SIP digest challenge flow. It does not mean
that the final call failed. The authenticated INVITE is the request that proceeds into the dialplan.

## B2BUA call establishment

After Asterisk accepts the authenticated caller leg, the `voip-lab` dialplan creates a second SIP
leg toward the registered contact for endpoint `1002`:

```text
1001                      Asterisk                      1002
  |                           |                           |
  | authenticated INVITE      |                           |
  |-------------------------->|                           |
  | 100 Trying                |                           |
  |<--------------------------|                           |
  |                           | INVITE                    |
  |                           |-------------------------->|
  |                           |                           |
  |                           | 180 Ringing               |
  |                           |<--------------------------|
  | 180 Ringing               |                           |
  |<--------------------------|                           |
  |                           |                           |
  |                           | 200 OK                    |
  |                           |<--------------------------|
  |                           | ACK                       |
  |                           |-------------------------->|
  | 200 OK                    |                           |
  |<--------------------------|                           |
  | ACK                       |                           |
  |-------------------------->|                           |
  |                           |                           |
  |       call established through Asterisk              |
```

The successful runtime snapshot from Commit 3 showed one `PJSIP/1001-*` channel and one
`PJSIP/1002-*` channel in state `Up`, with one active call.

## Media negotiation observation

During the manual trace, Linphone first offered an audio stream using `RTP/SAVP`. The current lab
endpoint configuration does not enable media encryption, so Asterisk rejected that attempt with
`488 Not Acceptable Here` because the offered media stream could not be negotiated.

Linphone then retried with `RTP/AVP`. That attempt succeeded and the negotiated audio codec observed
on the baresip side was `PCMU/8000`. Baresip reported both audio transmit and receive pipelines as
started and reported incoming RTP from Asterisk.

The successful call therefore demonstrates a normal client fallback from an unsupported secure-media
offer to a compatible unencrypted RTP offer in this lab configuration. The `488` response belongs
to the failed media offer, not to the final successful dialog.

## Call teardown

The observed successful call was terminated from the `1002` side. Asterisk completed teardown on
both independently managed call legs:

```text
1001                      Asterisk                      1002
  |                           |                           |
  |                           | BYE                       |
  |                           |<--------------------------|
  |                           | 200 OK                    |
  |                           |-------------------------->|
  |                           |                           |
  | BYE                       |                           |
  |<--------------------------|                           |
  | 200 OK                    |                           |
  |-------------------------->|                           |
```

After teardown, Asterisk reported zero active channels and zero active calls while both SIP contacts
remained registered and reachable.

## What the trace proves

The Phase 4 trace demonstrates all of the following protocol behavior:

- SIP digest challenge and authenticated INVITE handling on the caller leg;
- Asterisk dialplan routing from extension `1002` to the registered PJSIP contact;
- independent caller and callee dialogs created by Asterisk as a B2BUA;
- ringing propagation with `180 Ringing`;
- successful answer with `200 OK` and ACK completion on both legs;
- media negotiation to `PCMU/8000` over RTP for the successful call;
- clean BYE / `200 OK` teardown on both call legs;
- registrations surviving after the call ends.

## Security and evidence policy

Do not commit raw SIP logger captures from this lab. Such captures can contain dynamic digest
authorization values and media keying attributes. Commit only sanitized protocol summaries and
selected non-secret runtime state.
