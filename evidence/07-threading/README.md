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
- `final-runtime-acceptance.txt`: append-preserved final offline acceptance
  history. The latest revalidation is on committed HEAD `8c78677` and keeps
  live SIP/PJSUA2 shutdown boundaries explicitly unresolved.

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

## THINKPAD FINAL RUNTIME REVALIDATION — HEAD 8c78677

Recorded: 2026-10-10

- Automated offline acceptance: PASS.
- Canonical CTest: 147/147 PASS.
- Focused Phase 8: 12/12 PASS.
- Clang18 TSan: 18/18 PASS.
- PJSIP non-live regression: 45/45 PASS.
- GUI headless startup/exit: 40/40 PASS.
- Manual graphical GUI interaction: PASS over 111.47 seconds.
- Normal window X-close: PASS, exit code 0.
- Source checkout cleanliness: PASS.
- Live SIP registration/calls: NOT TESTED.
- Production PJSUA2 shutdown safety: BLOCKED/UNPROVEN.
- Final maintainer acceptance: PENDING.

Detailed sanitized per-run evidence:
`evidence/phase8/final-runtime-acceptance-8c78677.txt`.
