# Roadmap-required Phase 8 evidence

This directory uses the **exact** naming in `README_roadmap.md` section 11.
The previous `evidence/phase8/` directory is retained as a historical record.
Do not overwrite older evidence to make it look like a newer ThinkPad run.

- `stress-test.txt`: reference to observed ThinkPad MPMC stress run and
  deterministic 100,000-item invariants; copy full terminal logs only from
  the actual ThinkPad when retained and identify their source.
- `tsan-report.txt`: scope/limitations and observed Clang18 six-test ×
  three-run result from maintainer's Commit 19 log.
- `threading-diagram.png`: **descriptive code-level architecture diagram**,
  not a screenshot, execution trace, or sanitizer output. Graphviz source:
  `docs/architecture/threading-diagram.dot`. The generic Phase 8 worker is
  explicitly disconnected from real Qt/PJSUA2 runtime.

No manual live SIP callback-quiescence test has been demonstrated by these
artifacts. Production shutdown safety remains BLOCKED per Commit 19 audit.


## THINKPAD VERIFIED PHASE 8 ROADMAP CLOSURE V2

Recorded: 2026-10-10T09:16:37+07:00

ThinkPad observation:
- Roadmap exit verifier: PASS.
- Required threading artifacts: FOUND.
- Source boundaries: PASS.
- Overlay source integrity: PASS.
- Integration readiness: BLOCKED.
- Live external callback quiescence: UNPROVEN.
- Actual PJ_EBUSY recovery: UNPROVEN.

Original log:
`/tmp/phase8-roadmap-exit-v2.log`

Historical Commit 19 results:
- Focused Phase 8: 12/12 PASS.
- Clang18 TSan: 18/18 PASS.
- PJSIP regression: 45/45 PASS.
- Canonical CTest: 147/147 PASS.
- GitHub Actions: 3/3 PASS, maintainer reported.

These historical runtime results were not rerun by this
documentation-only closure verification.

Commit/push: PENDING.
Closure CI: PENDING.
Final maintainer acceptance: PENDING.
