# Phase 8 Commit 32 — Final Commit 31 runtime acceptance evidence

Commit 32 is documentation/evidence only. It records final runtime acceptance
observations for production Commit 31
`6b19d5ebcbd56991aa3c0e97b163780942b6a54e`.

It does not modify production C++, test logic, scripts, or PJSUA2 integration
policy.

## Current-head automated acceptance

Automated Phase 8 verification was rerun after Commit 31 had been committed and
pushed. The evidence is tied to HEAD
`6b19d5ebcbd56991aa3c0e97b163780942b6a54e`.

Observed results:

- canonical CTest: **147/147 PASS**;
- Qt registration worker regression: **10/10 PASS**;
- focused Phase 8 CTest: **12/12 PASS**;
- 100,000-event MPMC stress: **30 repeated runs PASS**;
- isolated Clang 18 TSan: **18/18 PASS**;
- PJSIP non-live regression: **45/45 PASS**;
- runtime harness self-tests: **6/6 PASS**;
- headless Qt/QML startup and normal exit: **40/40 PASS**.

## Manual GUI acceptance

Attempt 1 was retained as a failed acceptance-protocol attempt because the GUI
was closed before the required minimum observation interval.

Attempt 2 passed:

- accepted: `true`;
- observation: **93.77 seconds**;
- samples: **187**;
- exit code: **0**;
- RSS first/last/peak: **193568 / 196392 / 196392 KiB**;
- FD min/max: **30 / 31**;
- thread min/max: **23 / 24**.

The human operator confirmed the required GUI behavior and normal window-close
protocol.

## Production registration observation

Three independent runs of the production `voip-sip-register` path were executed
on the exact canonical source bytes subsequently committed as Commit 31.

Each run observed SIP registration status `200`, `active=yes`, and process exit
code `0`.

The sanitized evidence log was previously verified with SHA256:

`bd4970b23d2d6eea58f5e60c7ee7da6b2c6dccd49891c7507fc731a6d74d4287`

The presence of an ephemeral local Downloads copy is not required for this
documentation commit; the observed result and digest are preserved as
provenance.

This live registration observation is not an injected production-client
`PJ_EBUSY` recovery test.

## Controlled PJSUA2 busy recovery

Commit 30 separately records one controlled PJSUA2 2.17 lifecycle sequence:

`PJ_EBUSY` -> hangup -> zero call slots -> non-forced shutdown retry success.

That remains vendor-level lifecycle evidence rather than proof of all production
shutdown schedules.

## Safety boundary

The following remain unchanged:

- live external callback quiescence: **UNPROVEN**;
- production-client real `PJ_EBUSY` recovery: **UNPROVEN**;
- PJSUA2 integration readiness: **BLOCKED**.

Commit 31 retaining derived Account callback targets through
`Endpoint::libDestroy()` is an implementation-reviewed lifetime ordering. It is
not claimed to be a universal API-level callback-quiescence guarantee.

Final Phase 8 milestone acceptance, merge, and release remain explicit
maintainer decisions.
