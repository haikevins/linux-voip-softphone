# Phase 8 evidence index

Phase 8 remains **in progress**. Test evidence does not close the milestone.

- `automated-verification.txt`: preserved isolated verification for Commits
  1–4 and maintainer-observed ThinkPad gates. Commit 4 focused CTest 3/3,
  canonical 138/138, `c7a4243` pushed/synchronized (2026-10-09 log).
  Additional Commit 5 *isolated* results are recorded separately from the
  ThinkPad results; expected canonical count after this ZIP is 139.
- `stress-test.txt`: Commit 5 observed 100,000-event/4x4 MPMC integrity,
  FIFO, close/drain, failure injection, and repeated-stress test evidence.
- `manual-verification.txt`: SIP/GUI manual testing status; unchanged by
  these isolated concurrency tests.

Commit 5 canonical ThinkPad regression, ThreadSanitizer, Qt/PJSIP callback
thread-affinity audit, and real SIP/GUI manual verification are **not yet
performed/confirmed**. Record results only after running the corresponding
gates. Never include raw SIP auth, PCAP, SRTP keys, or passwords.
