# Phase 4 — SIP Call-Flow Experiment

## Goal

Commit 3 proves that the two registered SIP endpoints can establish and tear down a real call
through the Asterisk lab from Commit 1.

The runtime acceptance criteria are:

- endpoint `1001` remains registered and reachable;
- endpoint `1002` remains registered and reachable;
- dialing extension `1002` from `1001` creates one active call;
- Asterisk has one `PJSIP/1001-*` channel and one `PJSIP/1002-*` channel;
- both PJSIP channels reach state `Up` after endpoint `1002` answers;
- after hangup, Asterisk returns to zero active channels and zero active calls.

Protocol-level analysis of individual SIP messages belongs to the next Phase 4 commit. Raw SIP
logger output is intentionally not committed because it can contain authentication material and
media keying attributes.

## Lab topology

```text
Redmi 12 5G / Linphone Android / endpoint 1001
                  |
               Wi-Fi
                  |
                  v
ThinkPad / Ubuntu / Asterisk
                  ^
                  |
          loopback 127.0.0.1
                  |
ThinkPad / baresip / endpoint 1002
```

Asterisk acts as the B2BUA between the two SIP user agents. `direct_media=no` keeps Asterisk in the
call path, which makes the two call legs visible independently during the lab.

## Prerequisites

The registration verifier from Commit 2 must pass before starting the call:

```bash
sudo ./scripts/verify_sip_registration.sh --check
```

Keep baresip running for endpoint `1002`:

```bash
baresip -v
```

## Establish the call

From Linphone Android on endpoint `1001`, dial extension `1002`.

When baresip reports the incoming call, answer it with:

```text
a
```

Keep the call connected while running the call-flow verifier from another terminal:

```bash
sudo ./scripts/verify_sip_call.sh --check
sudo ./scripts/verify_sip_call.sh --capture
```

A successful check ends with:

```text
SIP call verification: PASS
```

The capture records only selected non-secret Asterisk state. It does not record raw SIP packets.

## Runtime state during the call

Asterisk should report two active PJSIP channels and one active call. The exact channel suffixes,
contact hashes, ports, bridge identifiers, timestamps, and RTT values are runtime-dependent and must
not be hard-coded into tests.

Useful manual inspection commands are:

```bash
sudo asterisk -rx "core show channels verbose"
sudo asterisk -rx "pjsip show channels"
sudo asterisk -rx "pjsip show contacts"
```

## Tear down the call

Hang up from baresip with:

```text
b
```

Then verify that Asterisk has no active call left while both endpoints remain registered:

```bash
sudo asterisk -rx "core show channels"
sudo asterisk -rx "pjsip show contacts"
```

Expected idle summary:

```text
0 active channels
0 active calls
```

Both contacts should still be present and available after the call ends.
