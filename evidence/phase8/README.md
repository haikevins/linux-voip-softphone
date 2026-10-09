# Phase 8 evidence index

Phase 8 is **in progress**. Evidence is not a milestone-closure claim.

- `automated-verification.txt`: observed isolated tests for Commits 1–4;
  maintainer-reported ThinkPad verification for Commits 1–3, including
  Commit 3 focused 2/2, full regression 135/135, and pushed `9358f8b`.
  Commit 4's canonical 138-entry regression is *not yet observed*.
- `manual-verification.txt`: SIP/GUI manual scope and untested status.

The 100,000-event stress run, ThreadSanitizer report, and real SIP/GUI manual
verification are **not yet performed**. Commit 4 adds three standalone CTest entries and a
guarded script to register those entries in the canonical suite; the script
has not been run on the ThinkPad in this evidence. Record observed results
after each new gate. Do not include raw SIP auth material, PCAP, or passwords.
