# Phase 8 evidence index

Phase 8 is **in progress**. Evidence is not a milestone-closure claim.

- `automated-verification.txt`: observed isolated tests for Commits 1–3;
  maintainer-reported canonical ThinkPad verification for Commits 1–2,
  including focused 1/1 and full 135/135 regressions. Commit 3's canonical
  results are still pending until this overlay is applied.
- `manual-verification.txt`: SIP/GUI manual scope and untested status.

The 100,000-event stress run, ThreadSanitizer report, and real SIP/GUI manual
verification are **not yet performed**. Phase 8 tests for Commit 3 are
currently standalone (two separate CTest entries), not part of the
canonical top-level 135-entry suite. Record observed results after each
new gate. Do not include raw SIP auth material, PCAP, or passwords.
