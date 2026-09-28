# Phase 4 — Final Verification

## Goal

Commit 5 closes Phase 4 by re-running the live Asterisk and SIP registration checks and by verifying
that the runtime and protocol evidence captured in the earlier Phase 4 commits is present and
internally consistent.

This final verification does not require another active call. The established-call state was already
captured in Commit 3; the final check verifies that evidence together with the current idle runtime
state.

## Acceptance criteria

The final verifier requires all of the following:

- the Asterisk runtime check passes;
- endpoints `1001` and `1002` are currently registered;
- both current contacts are reachable (`Avail`);
- Asterisk is idle with zero active channels and zero active calls;
- Commit 3 call evidence records two active PJSIP channels and one active call;
- the captured `PJSIP/1001-*` and `PJSIP/1002-*` channels were both in state `Up`;
- the sanitized protocol evidence records digest challenge, ringing, answer, ACK/BYE teardown,
  B2BUA behavior, and `PCMU/8000` media negotiation;
- Phase 4 documentation and evidence do not contain raw Digest authorization headers or inline
  media keying material.

## Run the final check

Keep Linphone endpoint `1001` and baresip endpoint `1002` registered, but do not place a call. Then
run:

```bash
sudo ./scripts/verify_phase4.sh --check
```

The expected final line is:

```text
Phase 4 final verification: PASS
```

## Capture final evidence

After the check passes, capture the final idle runtime state:

```bash
sudo ./scripts/verify_phase4.sh --capture
```

The command writes selected non-secret artifacts under `evidence/04-sip/final/`.

## Evidence policy

The final evidence intentionally stores selected Asterisk state and sanitized summaries only. Raw
PJSIP logger output is excluded because packet traces can contain dynamic Digest authorization
values and media keying attributes.
