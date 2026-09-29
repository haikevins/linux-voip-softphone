# Phase 4 — SIP Call Lab

## Purpose

Prove that the two registered SIP endpoints can establish and tear down a real call through the verified Asterisk lab.

The runtime acceptance criteria are:

- endpoint `1001` remains registered and reachable;
- endpoint `1002` remains registered and reachable;
- dialing extension `1002` from `1001` creates one active call;
- Asterisk exposes one `PJSIP/1001-*` channel and one `PJSIP/1002-*` channel;
- both PJSIP channels reach state `Up` after endpoint `1002` answers;
- after hangup, Asterisk returns to zero active channels and zero active calls while registrations remain.

Protocol semantics for the observed SIP messages belong in `docs/protocols/sip.md`. Raw SIP logger output is intentionally not committed because it can contain authentication material and media keying attributes.

## Lab topology

```text
Redmi / Linphone / endpoint 1001
                  |
               Wi-Fi/LAN
                  |
                  v
ThinkPad / Ubuntu / Asterisk
                  ^
                  |
          local/LAN path
                  |
ThinkPad SIP client / endpoint 1002
```

Asterisk acts as the B2BUA between the two SIP user agents. `direct_media=no` keeps Asterisk in the media path, making the two call legs independently observable.

## Prerequisites

Registration must pass before starting the call:

```bash
sudo ./scripts/phase4/verify-registration.sh --check
```

Keep the endpoint `1002` SIP client running and available to answer an incoming call.

## Establish the call

From endpoint `1001`, dial extension `1002` and answer the incoming call on endpoint `1002`.

Keep the call connected while running the verifier from another terminal:

```bash
sudo ./scripts/phase4/verify-call.sh --check
```

Expected final line:

```text
SIP call verification: PASS
```

## Verify the active call

Asterisk should report two active PJSIP channels and one active call. Exact channel suffixes, contact hashes, ports, bridge identifiers, timestamps, and RTT values are runtime-dependent and must not be hard-coded into tests.

Useful manual inspection commands are:

```bash
sudo asterisk -rx "core show channels verbose"
sudo asterisk -rx "pjsip show channels"
sudo asterisk -rx "pjsip show contacts"
```

Capture the selected non-secret established-call state with:

```bash
sudo ./scripts/phase4/verify-call.sh --capture
```

The capture records selected Asterisk state rather than raw SIP packets.

## Tear down the call

Hang up from either endpoint after verification, then confirm that Asterisk has no active call while both contacts remain registered:

```bash
sudo asterisk -rx "core show channels"
sudo asterisk -rx "pjsip show contacts"
```

Expected idle summary:

```text
0 active channels
0 active calls
```

Call-dialog lifetime and registration lifetime are independent; ending the call must not require removing the registered contacts.

## Evidence and protocol boundary

The active-call evidence belongs under `evidence/phase4/call-flow/`. Protocol interpretation of REGISTER/INVITE responses, B2BUA dialog behavior, SDP negotiation, and BYE teardown belongs in `docs/protocols/sip.md`.

Do not commit raw authentication-bearing SIP captures or media keying material.

## Completion criteria

The call lab is complete when the live call reaches the expected two-leg `Up` state, sanitized call evidence has been captured, teardown returns Asterisk to zero active calls/channels, and both endpoint registrations remain available.

## References

- `docs/phase4/README.md`
- `docs/architecture/sip-asterisk.md`
- `docs/protocols/sip.md`
- `evidence/phase4/call-flow/`
