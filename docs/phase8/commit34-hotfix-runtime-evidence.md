# Phase 8 Commit 34 — Hotfix current-head runtime acceptance evidence

Commit 34 is a documentation/evidence-only candidate recording final offline
runtime observations for hotfix HEAD
`b6c84ac48f9dbb9d442a446dc05bdd47ec1f08ca`.

The hotfix parent is Commit 32
`07ad46c46d905fab25b44ba660693c0468d2abe7`. The production change itself was committed separately before this
evidence refresh.

## Current-head GitHub CI

GitHub Actions completed all three configured workflows successfully on the
exact hotfix HEAD:

- Build: **SUCCESS**;
- Static Analysis: **SUCCESS**;
- Tests and Sanitizers: **SUCCESS**;
- aggregate result: **3/3 SUCCESS**.

The previously observed Static Analysis formatting / exception-escape failure is
therefore closed for the current HEAD.

## Automated offline acceptance

The Phase 8 final runtime harness was rerun after the hotfix was committed and
pushed.

Observed results:

- canonical CTest: **147/147 PASS**;
- Qt registration worker regression: **10/10 PASS**;
- focused Phase 8 CTest: **12/12 PASS**;
- 100,000-event MPMC stress: repeated gate **PASS**;
- isolated Clang 18 TSan: **18/18 PASS**;
- PJSIP non-live regression: **45/45 PASS**;
- runtime acceptance harness self-tests: **PASS**;
- headless GUI startup/normal exit: **40/40 PASS**;
- headless startup median: **191.04 ms**;
- headless startup P95: **202.22 ms**;
- headless startup maximum: **207.21 ms**.

## Manual GUI acceptance

The graphical manual gate passed on the same current-head acceptance run:

- accepted: `true`;
- observation: **92.97 seconds**;
- samples: **185**;
- process exit code: **0**;
- RSS first/last/peak: **193320 / 195792 / 200352 KiB**;
- FD min/max: **30 / 31**;
- thread min/max: **23 / 24**.

The operator confirmed the required preview-mode GUI behavior and closed the
application using the normal window close path after the harness requested it.

These values are runtime observations, not a memory-leak or long-duration
performance proof.

## Historical live evidence boundary

The following live observations remain useful but are historical:

- Commit 31 production registration/teardown: **3/3 PASS**;
- controlled PJSUA2 `PJ_EBUSY` -> hangup -> zero call slots -> retry-success:
  **LIVE OBSERVED / EVIDENCED**.

They were **not rerun on hotfix HEAD `b6c84ac`** by this offline acceptance.
Accordingly, this record does not claim new live SIP or universal teardown
proof for the current HEAD.

## Safety boundary

The following remain unchanged:

- live external callback quiescence: **UNPROVEN**;
- production-client real `PJ_EBUSY` recovery: **UNPROVEN**;
- PJSUA2 integration readiness: **BLOCKED**;
- maintainer Phase 8 acceptance: **PENDING**.

The hotfix does not establish an API-level universal callback-quiescence
guarantee.

## Evidence provenance

Frozen current-head summary SHA256:

`ba18bc197c402ee977a8db47ed8393cbf43ea3f6d0d2c4c9529b80288a5f0d3f`

Underlying retained artifact digests:

- automated run log:
  `c990e60789f867fc6e9fc6b74eea0dde7fca4c326b95607af2bbe9fe91156cd4`;
- acceptance status:
  `db137a7c8d0be734a0638e429fb695f6d68ba86209639e141f8fec72fa08b959`;
- GUI smoke results:
  `77c54f00626e300dad533f27cb4bbeb5313902e3f305a15b3cb28c95e7b23a06`;
- manual GUI results:
  `1a5afeb63923c84fffc89fc5959d3a3c8d04fc9647b0e04c41c6d8e951d6dd0e`;
- manual resource samples:
  `d0fde7911af5c87928073c25f3515c95db689c7bc72998a25ea2fd7726a87d97`;
- manual log:
  `67f882f9a04b65e360c27fa81ae37fff032418f8d9105e9a91dac39542e664d9`.

Final Phase 8 milestone acceptance, merge, and release remain explicit
maintainer decisions.
