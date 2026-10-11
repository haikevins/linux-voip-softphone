# Phase 8 — PJSUA2 shutdown and callback-lifetime safety

This document consolidates the Phase 8 ownership, shutdown-order, callback
activity, production lifetime, failure-path, callback-ingress,
shutdown-ownership, authorization and integration-contract reviews that were
previously split across per-commit documents.

It is a current design/audit summary, not a claim that PJSUA2 exposes a universal
callback-quiescence API.

## Historical progression

The ownership/lifetime work evolved in reviewed slices:

- Commit 7 (`cf31e2a`) introduced the Qt/PJSIP ownership audit entry gate.
- Commit 8 (`1404826`) reviewed real source ownership and Qt queued handoff.
- Commit 9 (`368f510`) changed outgoing/incoming call teardown ordering so the
  derived `Call` target remains alive while bounded call-slot cleanup is
  awaited.
- Commit 10 (`8bef4c7`) factored deterministic call-slot polling and its tests.
- Commit 11 (`f77d0b3`) introduced the standalone `CallbackActivityGate` model
  and explicitly distinguished local activity drain from external library
  quiescence.
- Commit 12 (`5d1e2c2`) added a production-source lifetime audit and mutation
  tests; the then-open shutdown risks were recorded as static risks rather than
  runtime failures.
- Commits 13–14 repaired CI formatting/tidy regressions without changing the
  PJSUA2 safety boundary.
- Commits 15–19 added deterministic failure-path, callback-ingress,
  ownership-policy, shutdown-authorization and fail-closed integration
  contracts.
- Commits 27–29 (`f555976`, `b8cca4c`, `4ace2be`) hardened production callback
  admission, fail-closed destruction and callback-owner lifetime.
- Commit 31 (`6b19d5e`) retained derived Account callback targets through
  successful `Endpoint::libDestroy()` and reset them only afterwards.
- Hotfix `b6c84ac` closed `noexcept` exception-escape paths around shutdown
  disconnection waits and restored GitHub CI to 3/3.

The former `docs/phase8/commit7-*` through `commit19-*` files are intentionally
removed from the current tree by consolidation. Their exact historical content
remains available from Git history and is reflected by the chronological
`evidence/phase8/` records.

## Current production lifetime model

The current reviewed implementation uses a shared callback context rather than
having callback wrappers borrow the owning SIP client by direct reference.
Callback entry is guarded by local activity/admission state and resolves the
owner through an atomic pointer. The owner is detached only after successful
safe teardown; unsafe destruction remains fail-closed.

For call clients, the important ordering is:

1. initiate call hangup when needed;
2. wait for disconnection/call-slot cleanup while the derived `Call` target is
   still alive;
3. close/drain local callback activity according to the production gate;
4. release the call wrapper only after the guarded shutdown path permits it;
5. perform non-forced account shutdown (`shutdown2(force=false)`);
6. retain the derived Account target through successful `Endpoint::libDestroy()`;
7. reset retained Account/Endpoint state only after successful library destroy.

If the required safety conditions are not met, destruction is not silently
continued. The production path is designed to retain state and fail closed
rather than pretend a failed shutdown is safe.

For registration, the same principle applies: callback entry uses the shared
context/activity gate, account shutdown is non-forced, the derived Account is
retained through successful library destroy, and owner detachment happens only
on the safe path.

## What local gates prove

Phase 8 has deterministic tests for:

- bounded call-slot polling and timeout behavior;
- callback activity admission/close/drain modeling;
- synthetic callback ingress races;
- stale-result and retry ownership policy;
- shutdown failure-path handling;
- shutdown-authorization policy;
- production source structure and mutation regressions;
- shutdown integration fail-closed behavior;
- Qt owner-thread and worker-to-QObject handoff;
- PJSIP non-live regression through the canonical CTest suite.

These gates are valuable because they catch regressions in source ordering and
local ownership contracts. They do **not** prove that an external PJSUA2 worker,
transaction timer, transport callback or media callback cannot enter after a
particular application-side drain point.

## PJSIP 2.17 source-backed lifecycle evidence

The reviewed PJSIP 2.17 implementation supports several important ordering
facts used by the production design:

- non-forced account shutdown can report `PJ_EBUSY` while calls are active;
- the application must hang up active calls and wait for disconnection before a
  non-forced retry can succeed;
- `Call`/Account wrappers carry callback targets through PJSUA user-data links;
- `Endpoint::libDestroy()` stops PJSUA worker threads early in library teardown;
- media teardown waits for media endpoint worker threads to quit;
- endpoint destruction occurs before account/buddy pools because transaction
  layer shutdown can still emit events that reach account/buddy callbacks.

Those implementation details support retaining derived Account targets through
`libDestroy()`. They are **implementation-level evidence**, not a documented
API-level guarantee that every possible external callback source has reached a
universal quiescence barrier at an earlier application-visible point.

## Remaining boundary

The following remain explicit:

```text
Live external callback quiescence: UNPROVEN
Production-client real PJ_EBUSY recovery: UNPROVEN
Production integration readiness: BLOCKED
```

The controlled Commit 30 experiment observed one real PJSUA2 `PJ_EBUSY`
recovery sequence, but it exercised a dedicated probe rather than forcing the
same timing through a production client's `stop()` method. The production
registration 3x observation on Commit 31 also did not inject `PJ_EBUSY`.

`PjsipShutdownAuthorization` remains a tested policy/contract boundary; it is
not treated as proof that external callback quiescence has been established.

## Current verification commands

The historical one-file-per-check wrappers are consolidated into
`scripts/phase8/verify-pjsip.sh`:

```bash
bash scripts/phase8/verify-pjsip.sh callback-activity
bash scripts/phase8/verify-pjsip.sh callback-ingress-races
bash scripts/phase8/verify-pjsip.sh cleanup-timeout
bash scripts/phase8/verify-pjsip.sh production-lifetime-audit
bash scripts/phase8/verify-pjsip.sh shutdown-failure-paths
bash scripts/phase8/verify-pjsip.sh shutdown-ownership-policy
bash scripts/phase8/verify-pjsip.sh shutdown-authorization
bash scripts/phase8/verify-pjsip.sh shutdown-integration-contract
bash scripts/phase8/verify-pjsip.sh shutdown-order
bash scripts/phase8/verify-pjsip.sh qt-ownership
bash scripts/phase8/verify-pjsip.sh live-evidence
```

`all-offline` runs the consolidated offline PJSIP/Qt gate set. It never executes
the real SIP lifecycle probe:

```bash
bash scripts/phase8/verify-pjsip.sh all-offline
```

The real lab probe remains explicit opt-in:

```bash
PHASE8_LIVE_SIP=1 bash scripts/phase8/run-pjsip-live-lifecycle-probe.sh
```

## Evidence policy

Historical evidence files retain the command and document paths that existed at
the time each observation was captured. They are not rewritten to make old
runs appear to have used the consolidated entry points. Current reproduction
commands belong in this document, `README.md`, and `evidence/phase8/README.md`.

No SIP password, raw Digest Authorization material, nonce/cnonce/response
material, SRTP inline key, or sensitive packet capture belongs in committed
evidence.
