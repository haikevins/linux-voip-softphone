# Phase 8 Evidence — Multithreading

## Status

Phase 8 evidence is merged/closure evidence, but final maintainer acceptance remains **PENDING**.

Current safety boundary:

- controlled PJSUA2 `PJ_EBUSY` recovery: live observed/evidenced in the recorded probe;
- late registration callbacks after `shutdown2`: none observed in that recorded run;
- live external callback quiescence: **UNPROVEN**;
- production-client real `PJ_EBUSY` recovery: **UNPROVEN**;
- PJSUA2 integration readiness: **BLOCKED**.

There is no separate Phase 8 release tag.

## Verification scope

Evidence covers queue/worker semantics, canonical CTest integration, 100,000-event MPMC stress, standalone TSan, Qt/PJSUA2 ownership review, call-slot cleanup, shutdown failure/authorization models, controlled PJSUA2 lifecycle observation, and offline GUI/runtime acceptance.

It does not prove universal race freedom, live external callback quiescence, production-client recovery from every real `PJ_EBUSY` timing, or production integration readiness.

## Environment

Recorded runs use the maintainer Linux/ThinkPad environment with C++20/CMake/Ninja/CTest, Qt 6 where required, PJPROJECT/PJSIP 2.17 for PJSIP gates, and Clang 18 for successful standalone TSan verification. Individual artifacts retain exact baseline/tool details when relevant.

## Automated verification

Recorded final/offline evidence includes:

- canonical CTest: 147/147 PASS;
- Qt worker repeat: 10/10 PASS;
- focused Phase 8 CTest: 12/12 PASS;
- 100,000-event MPMC stress: repeated PASS;
- Clang 18 standalone TSan: 18/18 PASS;
- PJSIP non-live regression: 45/45 PASS;
- GUI headless startup/exit: 40/40 PASS.

The controlled PJSUA2 lifecycle artifact separately records one real `PJ_EBUSY` recovery sequence. It does not upgrade the unresolved production safety boundary.

## Manual verification

Historical preview-mode GUI acceptance passed on recorded Phase 8 heads, including normal window-close exit. These runs did not establish live SIP callback quiescence.

`pjsip-live-lifecycle.txt` is a separate controlled live PJSUA2 observation. It records successful retry after an observed `PJ_EBUSY` and no late registration callback in that observation window only.

## Artifacts

Current/core evidence kept at the Phase 8 root:

- `automated-verification.txt`
- `manual-verification.txt`
- `stress-test.txt`
- `tsan-report.txt`
- `pjsip-live-lifecycle.txt`
- `final-runtime-acceptance-b6c84ac.txt`
- `final-runtime-acceptance-8c78677.txt`
- `final-runtime-acceptance-ba050bb.txt`

Roadmap closure anchors kept at the Phase 8 root:

- `roadmap-closure.md`
- `roadmap-final-runtime-acceptance.txt`
- `roadmap-stress-test.txt`
- `roadmap-tsan-report.txt`
- `threading-diagram.png`

Historical per-commit/checkpoint records are indexed by `history/README.md` and retained byte-for-byte under `history/`. They are kept for provenance, not because they are part of the current/core verification surface.

Moving these historical files changes only their current-tree location; their bytes and SHA256 values are preserved.

## Results

The generic concurrency foundation, focused stress/TSan coverage, Qt owner-thread handoff, and fail-closed shutdown models have recorded PASS evidence within their stated scopes.

The controlled live PJSUA2 probe observed one real busy-recovery sequence, but **live external callback quiescence remains UNPROVEN**, **production-client real `PJ_EBUSY` recovery remains UNPROVEN**, and **PJSUA2 integration readiness remains BLOCKED**. Final maintainer Phase 8 acceptance remains **PENDING**.

## Security and redaction

Do not commit SIP passwords, raw Digest Authorization material, nonce/response values, SRTP inline keys, sensitive packet captures, core dumps, or unrelated generated build trees. Live evidence must retain only sanitized facts needed to support the result.

## Reproduction

Primary entry points:

```bash
bash scripts/phase8/verify.sh
bash scripts/phase8/verify-roadmap-exit.sh
bash scripts/phase8/verify-final-runtime-acceptance.sh --automatic
```

The controlled live lifecycle probe remains explicit opt-in:

```bash
PHASE8_LIVE_SIP=1 bash scripts/phase8/run-pjsip-live-lifecycle-probe.sh
```

A reproduced run creates a new observation; do not overwrite historical evidence to make an older run appear current.

## References

- `docs/phase8/README.md`
- `docs/architecture/threading-model.md`
- `docs/phase8/pjsip-live-lifecycle.md`
- `docs/phase8/final-runtime-acceptance.md`
- `docs/phase8/roadmap-exit-review.md`
- `docs/interview/phase8-questions.md`
- `README_roadmap.md`
- `scripts/phase8/`
