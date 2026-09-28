# Phase 4 SIP call-flow evidence

This directory stores selected runtime evidence for the Phase 4 call-flow experiment.

Capture the evidence only while a call from endpoint `1001` to endpoint `1002` is established and
both PJSIP channels are in state `Up`:

```bash
sudo ./scripts/verify_sip_call.sh --capture
```

The capture writes:

- `channels-verbose.txt` — active Asterisk channel state;
- `pjsip-channels.txt` — active PJSIP channel state;
- `contacts.txt` — registered contact state during the call;
- `call-summary.txt` — capture metadata and acceptance summary.

Do not commit raw `pjsip set logger on` output. SIP traces can contain Digest authorization values,
SDP crypto attributes, dynamically generated identifiers, and other runtime material that is not
needed for this evidence set.
