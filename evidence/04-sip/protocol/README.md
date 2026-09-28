# Phase 4 SIP protocol evidence

This directory stores sanitized protocol evidence for the successful Phase 4 SIP call.

The evidence is intentionally textual rather than a raw PJSIP logger capture. Raw traces can contain
SIP digest authorization fields and media keying attributes, so they are excluded from the repository.

`call-flow-summary.txt` records the protocol sequence observed during the successful manual call from
endpoint `1001` to endpoint `1002` through Asterisk.
