# Phase 8 — roadmap exit audit (Commit 19 baseline)

**Status: PENDING FINAL MAINTAINER ACCEPTANCE.** This is an evidence/architecture
closure candidate, **not** a claim that live PJSUA2 teardown is safe.

- Branch: `phase/8-multithreading`
- Expected parent HEAD: `e98bc3418f76b15b4b568d603f5e4002cbd3a544`
- Source of requirements: `README_roadmap.md`, section 11 (Phase 8),
  section 23 (Definition of Done). The project source archive dated prior
  to this review was used to read the roadmap; recheck against the canonical
  ThinkPad checkout before signing off.

## Requirements-to-evidence matrix

| Roadmap item | Actual evidence / boundary | Review |
|---|---|---|
| ThreadSafeQueue, mutex, condition_variable | `src/concurrency/ThreadSafeQueue.hpp`, unit tests | Implemented |
| atomic | concurrency unit and stress cases | Implemented |
| worker thread | `src/concurrency/SerialTaskWorker.hpp` and tests | Implemented, isolated |
| 100,000 events, multi-producer/consumer | `tests/phase8/test_mpmc_stress.cpp` (4P/4C); original Commit 5 ThinkPad transcript and Commit 19 focused CTest | Observed in supplied logs; confirm canonical provenance |
| `-fsanitize=thread` | Clang18 standalone six C++ tests, 3 runs each, Commit 19 ThinkPad C9 | 18/18 PASS; not live Qt/PJSIP |
| `docs/architecture/threading-model.md` | Current as-built addendum plus historical sections | Updated |
| `evidence/07-threading/stress-test.txt` | Documented actual observation and test recipe | Added |
| `evidence/07-threading/tsan-report.txt` | Documented actual ThinkPad C9 18/18 | Added |
| `evidence/07-threading/threading-diagram.png` | Reproducible Graphviz `.dot` source | Added; code-level only |
| SIP worker -> core -> Qt handoff | `QtRegistrationBackend` + Qt queued delivery + `CallManager` owner-thread contract | Source/test-supported; **not** generic queue integration or live callback proof |

## Critical distinctions

The Qt worker and queued QObject event handoff were already present at the
Phase 7 boundary. The Phase 8 generic `ThreadSafeQueue` and `SerialTaskWorker`
were intentionally not grafted onto that path; do **not** claim otherwise.
The roadmap's separate SIP/Metrics/Logging threads diagram is an explanatory
model, not proof that dedicated metrics and logging workers were implemented.

The current source integration audit still intentionally reports:

```text
Integration readiness: BLOCKED
Live external callback quiescence: UNPROVEN
Actual PJ_EBUSY recovery: UNPROVEN
```

That last field remains **production-integration scoped**: the static audit
does not consume live lab evidence and no real `PJ_EBUSY` has been injected
through a production client's `stop()` method. Separately, the controlled
Commit 30 live PJSUA2 2.17 experiment at baseline `4ace2be` observed this real
vendor-runtime sequence:

```text
ACTIVE_CALL_SLOTS=1
FIRST_SHUTDOWN=PJ_EBUSY
CALL_SLOTS_AFTER_HANGUP=0
SECOND_SHUTDOWN=SUCCESS
```

No registration callback (`onRegState`) was observed at successful shutdown
return, during the 1.5-second grace interval, or through `libDestroy()` in that
recorded run.
This narrows the runtime uncertainty around non-forced account shutdown, but it
does not establish external callback quiescence or universal production
teardown safety. Those remaining risks must be triaged explicitly rather than
silently declared solved.

## Verification and sign-off

Run `bash scripts/phase8/verify-roadmap-exit.sh` on the **actual canonical
ThinkPad checkout** after applying the exact overlay. It checks the roadmap,
source paths, static thread contracts, diagram, evidence, baseline and
fail-closed integration audit without modifying production source.

The archived ThinkPad logs of Commit 19 show: focused 12/12, Clang18 TSan
18/18, Qt ownership PASS, PJSIP 45/45, canonical CTest 147/147 and GitHub
Actions 3/3 as reported by the maintainer. No new runtime test results are
claimed by this closure ZIP. New test runs and GitHub checks for a future
closure commit remain **PENDING** until actually observed.

Accept Phase 8 educational/multithreading milestone **only after** the
ThinkPad verifier PASS, confirming the evidence aligns with the current source,
reviewing the outstanding PJSUA2 risks and deciding whether the absence of
new generic-queue-to-GUI wiring is acceptable for the roadmap's objective.
No merge to `main` or release tag is authorized here.

## Closure verifier repeatability

The Phase 8 exit verifier checks that the reviewed Commit 19 baseline
(`e98bc3418f76b15b4b568d603f5e4002cbd3a544`) is an ancestor of the
checked-out HEAD, not that HEAD remains frozen at Commit 19. Thus the same
read-only verifier remains usable on the future closure commit. This check
does **not** signal maintainer acceptance or prove live PJSUA2 teardown safety.


## THINKPAD VERIFIED PHASE 8 ROADMAP CLOSURE V2

Recorded: 2026-10-10T09:16:37+07:00

- Branch: `phase/8-multithreading`.
- Parent HEAD: `e98bc3418f76b15b4b568d603f5e4002cbd3a544`.
- Closure overlay v2 SHA256:
  `7eb4ec4525571a0b281d01c0c49154c0c1940e51d3070dfe573ddbb5822679af`.
- Overlay verification: PASS (1 modified + 7 added).
- Roadmap artifact inventory: PASS.
- Source contract inventory: PASS.
- Original Commit 19 regression results: retained without rerunning.
- Closure audit log: `/tmp/phase8-roadmap-exit-v2.log`.
- Commit/push: PENDING at evidence capture.
- GitHub Actions for closure commit: PENDING.
- Final maintainer acceptance: PENDING.

The closure verifies the multithreading roadmap artifacts
and source contracts. It does not establish safe live PJSUA2
shutdown or prove end-to-end integration of the generic queue.

Integration readiness: BLOCKED.
External callback quiescence: UNPROVEN.
Controlled PJSUA2 PJ_EBUSY recovery: LIVE OBSERVED / EVIDENCED.
Production-client recovery from an injected real PJ_EBUSY: UNPROVEN.
