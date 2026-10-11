# Phase 8 — Controlled live PJSUA2 lifecycle evidence

This document preserves the Commit 30 reproducible, opt-in live PJSUA2 2.17 lifecycle probe and
sanitized evidence from the controlled Asterisk lab. It is an evidence/harness
commit, not a production shutdown rewrite and not authorization to merge Phase
8.

## Recorded observation

The probe registered extension `1002`, created one outgoing call slot toward
`1001`, then called `Account::shutdown2(force=false)` while the slot was still
active. PJSUA2 returned `PJ_EBUSY`. The probe hung up, waited for the derived
`Call` DISCONNECTED callback to complete and for `pjsua_call_get_count()` to
reach zero, released the call wrapper, and retried non-forced account shutdown.
The retry succeeded.

The sanitized evidence records:

```text
REGISTERED=yes status=200
ACTIVE_CALL_SLOTS=1
FIRST_SHUTDOWN=PJ_EBUSY
CALL_SLOTS_AFTER_HANGUP=0
SECOND_SHUTDOWN=SUCCESS
ACCOUNT_CALLBACKS_ACTIVE_AT_RETURN=0
ACCOUNT_CALLBACKS_DURING_GRACE=0
POST_SHUTDOWN_CALLBACKS=0
LIB_DESTROY=SUCCESS
POST_LIBDESTROY_CALLBACKS=0
PROBE_RESULT=PASS
```

This is a **controlled PJSUA2 `PJ_EBUSY` recovery sequence observed live**. It
narrows a previous vendor-runtime uncertainty. It does not prove that every
production shutdown timing will recover identically.

## Callback boundary

The recorded counters cover `ProbeAccount::onRegState` registration
callbacks. No registration callback was active when the successful
`shutdown2()` returned, no new registration callback was observed during the
following 1.5-second grace interval, and no post-shutdown registration callback
was observed through `Endpoint::libDestroy()` in the recorded run.

Live external callback quiescence: **UNPROVEN**.

Zero observed callbacks in one controlled run is not an external callback
quiescence contract. The experiment does not cover adversarial callback timing,
registration refresh overlap, incoming-call teardown, remote disconnect races,
transport failure, or every thread schedule that PJSUA2 may produce.

## Production integration boundary

Production integration readiness: **BLOCKED**.

Commits 27–29 hardened the source lifetime structure, fail-closed destruction,
and callback-owner lifetime. Commit 30 does not wire
`PjsipShutdownAuthorization`, does not claim a provider for the external
quiescence receipt, and does not change the production SIP clients. The live
probe directly exercises PJSUA2 objects rather than forcing an actual
`PJ_EBUSY` through a production client's `stop()` method.

Accordingly:

- controlled PJSUA2 `PJ_EBUSY` recovery behavior: **LIVE OBSERVED / EVIDENCED**;
- late registration callback (`onRegState`) after successful shutdown in the
  recorded run: **NONE OBSERVED**;
- production-client recovery from an injected real `PJ_EBUSY`: **UNPROVEN**;
- external callback quiescence as a general lifetime guarantee: **UNPROVEN**;
- full live SIP/media acceptance: **NOT PROVEN**.

## Reproduction and security

The live runner is deliberately outside the normal offline aggregate:

```bash
PHASE8_LIVE_SIP=1 bash scripts/phase8/run-pjsip-live-lifecycle-probe.sh
```

The script normally requires a clean `phase/8-multithreading` checkout and
PJSIP 2.17, uses `pkg-config --libs --static libpjproject` for this
installation, prompts for the SIP password when necessary, and redacts common
Digest fields from the saved temporary log. `PHASE8_ALLOW_DIRTY=1` exists only
for guarded pre-commit validation and verifies that the live run does not
change the pre-existing working-tree state. The target extension may ring briefly and should not be
answered for this specific shutdown-busy experiment.

The offline evidence verifier is safe to run without credentials or network:

```bash
bash scripts/phase8/verify-pjsip.sh live-evidence
```

Do not commit passwords, raw `Authorization: Digest` material, nonce/cnonce or
response values, SRTP inline keys, or raw packet captures.

## Evidence

The canonical sanitized summary is:

```text
evidence/phase8/pjsip-live-lifecycle.txt
```

Its source temporary log had SHA-256
`77c62f614fef037c81aa254ebab2352a557197d2c278a6927a90084d93d481f5`
and was intentionally not added to the repository.
