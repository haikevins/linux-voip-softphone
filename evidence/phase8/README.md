# Phase 8 evidence index

Phase 8 remains **in progress**. Test evidence does not close the milestone.

- `automated-verification.txt`: preserved isolated verification for Commits
  1–4 and maintainer-observed ThinkPad gates. Commit 4 focused CTest 3/3,
  canonical 138/138, `c7a4243` pushed/synchronized (2026-10-09 log).
  Commit 5 ThinkPad logs subsequently confirmed focused 4/4, 100/100
  stress repeats, canonical 139/139 and clean push at `5979869`. Additional
  canonical evidence was appended on the maintainer's ThinkPad.
- `stress-test.txt`: Commit 5 observed 100,000-event/4x4 MPMC integrity,
  FIFO, close/drain, failure injection, and repeated-stress test evidence.
- `tsan-report.txt`: Commit 6 isolated GCC 14 TSan evidence, ThinkPad
  GCC 13 runtime BLOCKED diagnosis, and ThinkPad Clang 18 successful
  instrumented verification (four tests x three executions).
- `manual-verification.txt`: SIP/GUI manual testing status; unchanged by
  these isolated concurrency tests.

ThinkPad Clang 18 standalone ThreadSanitizer has been verified.
Qt/PJSIP callback thread-affinity audit and real SIP/GUI manual
verification are **not yet performed/confirmed**. Record
results only after running the corresponding gates. Never include raw SIP
auth, PCAP, SRTP keys, or passwords.

- `ownership-source-review.txt`: Commit 8 source-level Qt/PJSIP audit findings,
  risks requiring runtime confirmation, and the focused Qt regression gate.
  **Prepared only** until maintainer reports canonical results.
