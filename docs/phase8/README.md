# Phase 8 — Multithreading

**Current final evidence — hotfix HEAD / Commit 34 documentation candidate**

The current Phase 8 branch HEAD is
`b6c84ac48f9dbb9d442a446dc05bdd47ec1f08ca`, the static-analysis hotfix whose parent is Commit 32
`07ad46c46d905fab25b44ba660693c0468d2abe7`.

GitHub CI completed **3/3 SUCCESS** on this exact HEAD: Build, Static Analysis,
and Tests and Sanitizers all passed. Automated offline final runtime acceptance
also passed. The manual GUI gate passed with `accepted=true`, **92.97 seconds**
of observation, **185 samples**, and normal process exit `0`.

The current runtime record is
`evidence/phase8/final-runtime-acceptance-b6c84ac.txt`.

The earlier `final-runtime-acceptance-6b19d5e.txt` and
`final-runtime-acceptance-8c78677.txt` records remain unchanged historical
evidence. The Commit 31 production registration 3x observation and Commit 30
controlled PJSUA2 `PJ_EBUSY` experiment were not rerun by this current-head
offline acceptance.

Live external callback quiescence remains **UNPROVEN**, production-client real
`PJ_EBUSY` recovery remains **UNPROVEN**, and PJSUA2 integration readiness
remains **BLOCKED**. Final milestone sign-off, merge, and release remain
explicit maintainer decisions.

## Status

Phase 8 is a **closure candidate, not yet merged or released**. The latest
final offline acceptance was rerun on `phase/8-multithreading` at committed
HEAD `8c78677`, with Phase 7 release `v0.8-qt-gui` as the starting milestone.

The multithreading roadmap implementation, stress coverage, standalone
ThreadSanitizer coverage, ownership audits, and final offline GUI runtime
acceptance have recorded ThinkPad evidence. Final maintainer phase sign-off is
still pending. Production PJSUA2 shutdown safety remains deliberately
**BLOCKED/UNPROVEN** because external callback quiescence is not established.
A controlled live PJSUA2 2.17 experiment at Commit 29 baseline `4ace2be`
observed the `PJ_EBUSY` -> hangup -> zero call slots -> non-forced shutdown
retry-success sequence, but that observation is not a universal production
lifetime guarantee.

Do not describe the phase as proving universal race freedom, live SIP teardown
safety, or integration of the generic queue into the deployed Qt event path.

## Objective

Make the softphone concurrency model explicit and testable through:

- a reusable C++20 thread-safe queue;
- deterministic serial worker ownership and shutdown behavior;
- high-contention producer/consumer stress testing;
- independent ThreadSanitizer verification;
- explicit Qt/PJSUA2 ownership and lifetime review;
- fail-closed modeling of unsafe PJSUA2 shutdown cases; and
- repeatable offline GUI/runtime acceptance without changing Phase 7 SIP
  behavior.

The objective is to improve concurrency reasoning and verification without
pretending that all PJSUA2 or Qt operations should be moved to newly created
threads.

## Scope

In scope:

- `ThreadSafeQueue<T>` queue semantics, cancellation, close/drain behavior,
  producer/consumer integrity, and ownership requirements;
- `SerialTaskWorker` FIFO execution, exception containment, lifecycle, and
  non-reentrant task handoff;
- canonical CTest registration of Phase 8 tests;
- 100,000-event MPMC stress and repeated stress execution;
- opt-in standalone TSan with documented toolchain boundaries;
- Qt/PJSUA2 source ownership and callback-lifetime audits;
- bounded call-slot cleanup and deterministic cleanup policy tests;
- callback activity, shutdown failure-path, stale-result, and external-ingress
  race models;
- shutdown-authorization policy and integration contract checks;
- roadmap-required threading evidence and final offline GUI acceptance.

Out of scope:

- new user-facing call features or SIP account features;
- replacing Qt queued connections with the generic Phase 8 queue;
- claiming dedicated Metrics/Logging runtime threads exist merely because the
  roadmap diagram illustrates them;
- live SIP registration/call verification as part of the offline final runtime
  gate;
- treating one controlled live PJSUA2 recovery observation as proof of
  universal callback quiescence or production-client shutdown safety;
- changing or recreating the Phase 7 tag `v0.8-qt-gui`.

## Deliverables

### Concurrency foundation — Commits 1–6

- `9f22bdd` — `ThreadSafeQueue<T>` foundation.
- `a936437` — stop-aware queue cancellation.
- `9358f8b` — `SerialTaskWorker` infrastructure.
- `c7a4243` — canonical CTest integration and ownership tests.
- `5979869` — 100,000-event MPMC stress and failure injection.
- `ae870ff` — standalone ThreadSanitizer verification.

The generic queue/worker remains isolated from the production Qt/PJSUA2 event
path. This is intentional and is part of the verified phase boundary.

### Ownership and lifetime hardening — Commits 7–19

The detailed implementation/audit record is preserved in the phase-specific
supplemental documents:

- `commit7-ownership-audit.md`
- `commit8-source-ownership-review.md`
- `commit9-pjsip-shutdown-order.md`
- `commit10-pjsip-cleanup-timeout.md`
- `commit11-callback-activity.md`
- `commit12-pjsip-production-lifetime-audit.md`
- `commit13-ci-clang-format-hotfix.md`
- `commit14-clang-tidy-regression-hotfix.md`
- `commit15-pjsip-shutdown-failure-paths.md`
- `commit16-pjsip-callback-ingress-races.md`
- `commit17-pjsip-shutdown-ownership-policy.md`
- `commit18-pjsip-shutdown-authorization.md`
- `commit19-pjsip-shutdown-integration-contract.md`

These commits add source-level/runtime-model evidence and fail-closed contracts
around PJSUA2 shutdown. They do **not** establish safe live PJSUA2 object
destruction after all shutdown failures.

### Roadmap closure and offline acceptance — Commits 20–21

- `0cd1a7b` — roadmap threading evidence and exit review.
- `f353283` — final offline GUI runtime acceptance harness/evidence.

The final offline acceptance records canonical CTest, repeated Qt worker tests,
focused Phase 8 tests, MPMC stress, Clang 18 TSan, non-live PJSIP regression,
roadmap audit, GUI headless startup/exit, and human preview-mode GUI
interaction. Live SIP shutdown is explicitly outside that acceptance scope.

### Closure compliance and acceptance hardening — Commits 22–25

- `ac6f95f` — align Phase 8 closure documentation/evidence structure.
- `be8022c` — enforce docs/evidence quality gates and add the Phase 8 aggregate
  verifier.
- `2ca50e2` — keep verification from polluting the source checkout with Python
  bytecode.
- `8c78677` — flush manual-acceptance prompts so the normal X-close instruction
  is visible while output is piped through `tee`.

The final automated and 90-second manual GUI acceptance were rerun after these
corrections on committed HEAD `8c78677`; the source checkout remained clean.

### Production teardown hardening and controlled live evidence — Commits 27–30

- `f555976` — gate production callbacks during SIP teardown.
- `b8cca4c` — fail closed on unsafe SIP destruction.
- `4ace2be` — isolate SIP callback owner lifetime through shared callback
  context and detach.
- Commit 30 candidate — add an explicit opt-in PJSUA2 2.17 live lifecycle
  probe and preserve sanitized evidence from the controlled Asterisk lab.

The live experiment observed a real `PJ_EBUSY` from
`shutdown2(force=false)` while one call slot was active, followed by hangup,
zero call slots, and a successful retry. No late registration callback (`onRegState`) was observed at shutdown return,
during a 1.5-second grace interval, or through `libDestroy()` in that run. External callback quiescence remains **UNPROVEN**
and production integration readiness remains **BLOCKED**.

## Architecture and design

The authoritative architecture document is
[`docs/architecture/threading-model.md`](../architecture/threading-model.md).
Its key boundary is:

```text
Qt/PJSUA2 production path
    QtRegistrationBackend std::jthread
        -> RegistrationSession / PJSUA2 client
        -> QMetaObject::invokeMethod(..., Qt::QueuedConnection)
        -> GuiViewModel / CallManager owner thread

Generic Phase 8 path
    ThreadSafeQueue<T>
        -> SerialTaskWorker
        -> standalone concurrency/stress/TSan tests
```

Those are separate paths. Phase 8 does not claim the second path replaced the
first.

For PJSUA2 lifetime work, bounded call-slot cleanup and shutdown authorization
are defensive contracts. A zero active-call count or completed callback model
is not equivalent to proving vendor-library callback quiescence.

## Build and run

Focused queue/worker verification:

```bash
bash scripts/phase8/verify-queue.sh
```

Repeated MPMC stress:

```bash
PHASE8_STRESS_REPEATS=100 \
    bash scripts/phase8/verify-stress.sh
```

Standalone Clang 18 ThreadSanitizer verification:

```bash
CXX="$(command -v clang++-18)" \
PHASE8_TSAN_BUILD_DIR="$PWD/build/phase8-tsan-clang18-debug" \
PHASE8_TSAN_REPEATS=3 \
    bash scripts/phase8/verify-tsan.sh
```

Roadmap/source/evidence exit audit:

```bash
bash scripts/phase8/verify-roadmap-exit.sh
```

Offline verification of the committed live-lifecycle evidence summary:

```bash
bash scripts/phase8/verify-pjsip-live-lifecycle-evidence.sh
```

The real SIP experiment is explicit opt-in and is not part of the offline
aggregate:

```bash
PHASE8_LIVE_SIP=1 bash scripts/phase8/run-pjsip-live-lifecycle-probe.sh
```

Final offline automatic acceptance:

```bash
bash scripts/phase8/verify-final-runtime-acceptance.sh --automatic
```

The human GUI stage must be run from a graphical desktop and requires the
actual evidence directory printed by the automatic stage. See
`final-runtime-acceptance.md` for the exact procedure. It deliberately starts
the GUI without `--sip-register` and therefore does not require SIP secrets.

## Verification

Recorded ThinkPad results from the final rerun on committed HEAD `8c78677`
include:

- documentation structure: **PASS**;
- evidence structure: **PASS**;
- source checkout cleanliness after verification: **PASS**;
- canonical CTest: **147/147 PASS**;
- Qt worker/event-handoff repeat: **10/10 PASS**;
- Phase 8 focused tests: **12/12 PASS**;
- 100,000-event MPMC stress: **30/30 PASS** in the final acceptance run, with
  earlier 100/100 repeated stress evidence retained;
- Clang 18 standalone TSan: **18/18 PASS** (six tests × three executions);
- PJSIP non-live regression: **45/45 PASS**;
- GUI headless startup/exit: **40/40 PASS** (median 192.75 ms, P95 202.69 ms,
  maximum 214.44 ms);
- human preview-mode GUI interaction: **PASS** over 111.47 seconds;
- normal window X-close with exit code 0: **PASS**;
- manual resource observation: 222 samples, RSS 193376/198280/199628 KiB
  first/last/peak, FD range 30–33, thread range 23–24.

These results are scoped. The later controlled live lifecycle experiment also
recorded SIP 200 registration, one active call slot, real `PJ_EBUSY`, zero
call slots after hangup, and successful non-forced shutdown retry. It did not observe a late registration callback (`onRegState`) in that run. Together, the evidence still does
not prove:

- full live SIP/media teardown safety;
- production PJSUA2 external callback quiescence;
- production-client recovery from every actual `PJ_EBUSY` timing;
- universal race freedom;
- memory leak freedom; or
- sustained rendering/FPS performance.

Quality/documentation structure gates must also pass before Phase 8 is closed.

## Evidence

The historical per-commit Phase 8 evidence index is:

```text
evidence/phase8/
```

Roadmap-required final threading artifacts are retained separately at the
exact path named by `README_roadmap.md`:

```text
evidence/07-threading/
├── README.md
├── stress-test.txt
├── tsan-report.txt
├── threading-diagram.png
└── final-runtime-acceptance.txt
```

The `evidence/phase8/` history must not be overwritten to make earlier runs
look like later runs. The controlled live lifecycle summary is preserved as
`evidence/phase8/pjsip-live-lifecycle.txt`. `evidence/07-threading/` remains
the historical roadmap closure view and is not rewritten to make its earlier
offline acceptance appear to have included this later live experiment.

No raw SIP authentication material, passwords, PCAPs, or SRTP inline key
material belong in committed evidence.

## Interview focus

The review-oriented explanation for this phase is
[`docs/interview/phase8-questions.md`](../interview/phase8-questions.md).
Important topics include:

- why the generic queue is not automatically a replacement for Qt queued
  connections;
- the difference between queue shutdown, worker shutdown, and PJSUA2 object
  lifetime;
- what the 100,000-event MPMC stress test proves and does not prove;
- why isolated TSan coverage is useful but limited;
- how owner-thread rules are enforced for `CallManager` and Qt handoff;
- why call-slot count is not callback quiescence; and
- why Phase 8 records `BLOCKED/UNPROVEN` instead of manufacturing a PASS for
  live PJSUA2 teardown.

## Exit criteria

Roadmap implementation/evidence criteria currently have these states:

| Criterion | State |
|---|---|
| `ThreadSafeQueue`, mutex, condition variable, atomic | Implemented/tested |
| Owned serial worker thread | Implemented/tested in isolation |
| 100,000 events with multiple producers/consumers | PASS evidence |
| ThreadSanitizer where feasible | Clang 18 standalone PASS evidence |
| `docs/architecture/threading-model.md` | Present; schema compliance required |
| `evidence/07-threading/stress-test.txt` | Present |
| `evidence/07-threading/tsan-report.txt` | Present |
| `evidence/07-threading/threading-diagram.png` | Present |
| Offline GUI/runtime acceptance | PASS evidence |
| Phase documentation/evidence structure gates | PASS on final rerun |
| Final maintainer Phase 8 sign-off | PENDING |
| Merge to `main` / release tag | NOT STARTED |

Production PJSUA2 shutdown integration remains `BLOCKED/UNPROVEN` even after
the controlled `PJ_EBUSY` recovery observation. The live run narrows one
runtime uncertainty; it does not supply the missing external callback
quiescence contract or prove production-client shutdown under every race. The
maintainer must explicitly decide whether additional live PJSUA2 work is
required before Phase 8 sign-off.

## References

- [`README_roadmap.md`](../../README_roadmap.md), Phase 8 / multithreading.
- [`docs/architecture/threading-model.md`](../architecture/threading-model.md).
- [`docs/architecture/threading-diagram.dot`](../architecture/threading-diagram.dot).
- [`docs/phase8/roadmap-exit-review.md`](roadmap-exit-review.md).
- [`docs/phase8/final-runtime-acceptance.md`](final-runtime-acceptance.md).
- [`docs/interview/phase8-questions.md`](../interview/phase8-questions.md).
- [`evidence/phase8/README.md`](../../evidence/phase8/README.md).
- [`evidence/07-threading/README.md`](../../evidence/07-threading/README.md).
- `tests/phase8/`.
- `scripts/phase8/`.

## Next phase

Do not start Phase 9 from this branch until Phase 8 is explicitly accepted,
merged to `main`, synchronized, and (if the project release plan requires it)
tagged.

The roadmap next phase is Phase 9 — debugging, memory, and core dump. The open
production PJSUA2 shutdown risks from Phase 8 must remain visible as carried
technical debt unless they are resolved with real evidence before Phase 8
closure.


<!-- PHASE8_MANUAL_ACCEPTANCE_BA050BB -->

### Final manual runtime acceptance refresh

A final graphical GUI acceptance run was completed on parent runtime HEAD
`ba050bb9bcdd01391e2860b16e2f273c824b3ddc`.

Result: **PASS**.

The preview/no-SIP UI remained responsive through the required interaction
and idle-soak checks and exited normally through the window close path.

The complete observed record is:

`evidence/phase8/final-runtime-acceptance-ba050bb.txt`

This refresh is offline GUI evidence only. It does not establish live SIP
acceptance, external callback quiescence, or universal PJSUA2 shutdown safety.
Those boundaries remain explicitly **BLOCKED/UNPROVEN** where already
documented.
