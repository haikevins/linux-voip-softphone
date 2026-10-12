# Phase 8 — Multithreading

## Status

**PENDING** — Phase 8 is merged to `main` and its automated verification passes, but final maintainer acceptance has not been recorded.

The unresolved production boundary is unchanged:

- controlled PJSUA2 `PJ_EBUSY` recovery was observed in the recorded live probe;
- no late registration callback was observed after `shutdown2` in that recorded run;
- P8-R2B observed real `PJ_EBUSY` and bounded recovery on a production-owned outgoing Account in one operator-approved lab run (2026-10-12);
- external callback quiescence is **UNPROVEN**, as are other production-client shutdown scenarios;
- PJSUA2 integration readiness is **BLOCKED**.

There is no separate Phase 8 release tag.

## Objective

Make concurrency ownership and shutdown behavior explicit and testable without changing the Phase 7 user-facing call feature set.

## Scope

Included in this phase:

- `ThreadSafeQueue<T>` close, cancellation, FIFO, and drain semantics;
- owned `SerialTaskWorker` lifecycle and exception containment;
- repeated 100,000-event MPMC stress;
- ThreadSanitizer verification;
- Qt owner-thread handoff;
- PJSUA2 call-slot/callback/shutdown lifetime contracts;
- one controlled live PJSUA2 lifecycle observation.

Deliberately excluded:

- replacing Qt queued delivery with the generic queue;
- claiming universal PJSUA2 callback quiescence or teardown safety;
- treating offline verification or merge to `main` as final Phase 8 acceptance.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| Generic queue | Own MPMC FIFO, close, cancellation, and drain semantics | `src/concurrency/ThreadSafeQueue.hpp` |
| Serial worker | Own one worker thread and accepted-work drain | `src/concurrency/SerialTaskWorker.hpp` |
| Unit/integration tests | Exercise deterministic behavior within canonical CTest registration | `tests/unit/`, `tests/integration/` |
| Account shutdown invocation | Share fail-closed error handling; exercise a fake `PJ_EBUSY`-like error without a SIP server | `src/pjsip/PjsipAccountShutdownAttempt.hpp`, `tests/unit/test_pjsip_account_shutdown_attempt.cpp` |
| Offline verification | Run concurrency, 100,000-event stress, and TSan checks | `scripts/phase8/verify.sh`, `scripts/phase8/verify-threading.sh` |
| Live probe | Record bounded PJSUA2 lifecycle observations through explicit opt-in | `scripts/phase8/run-pjsip-live-lifecycle-probe.sh` |
| Production live stop/reuse lab (P8-R2A) | Build an opt-in outgoing-client harness; record only allowlisted outcome markers | `labs/pjsip/production-stop-reuse/`, `scripts/phase8/run-production-stop-reuse-lab.sh` |
| Real production Account busy/recovery lab (P8-R2B) | Opt-in active-call `shutdown2(false)` on production-owned Account; guarded real-status and same-object restart observation | `labs/pjsip/production-busy-recovery/`, `scripts/phase8/run-production-busy-recovery-lab.sh` |
| P8-R3A offline callback ingress audit | Fail on unreviewed callback overrides; exercise rejected in-flight local entries | `tests/integration/test_phase8_r3_callback_target_contract.py`, `tests/unit/test_pjsip_callback_activity.cpp` |
| Architecture | Document Qt/PJSUA2 ownership and unproven shutdown boundaries | `docs/architecture/threading-model.md` |
| Evidence | Preserve sanitized stress, TSan, GUI, and controlled live observations | `evidence/phase8/README.md` |

## Architecture and design

`docs/architecture/threading-model.md` is the architecture source of truth.

The production GUI retains the Phase 7 model:

```text
QtRegistrationBackend session worker
        -> PJSUA2 session/client operations
        -> Qt::QueuedConnection
        -> QObject/UI owner thread
```

The generic concurrency primitives are independently reusable:

```text
producers -> ThreadSafeQueue<T> -> consumers
producers -> SerialTaskWorker   -> one owned worker thread
```

These paths are related by ownership principles, not by an assertion that the generic queue transports every production Qt/PJSUA2 callback.

### P8-R3C: one-package real PJPROJECT dispatch observation

One review patch adds the lab-only vendor hook diff, a real outgoing-client
observer, a guarded one-call runner, sanitized marker validation, canonical
offline CTest tests, and architecture/interview/evidence notes. It does **not**
change default production shutdown or vendor libraries. Two instrumented callback
paths are `on_call_state` and `on_call_media_state`; only the outgoing
`release_disconnected_call()` path is exercised dynamically. The other
previously inventoried Call release sites still require separate evidence.

The runner creates an isolated instrumented source/build/install under
`build/phase8-r3c-vendor-lab`, requiring the clean PJPROJECT tag `2.17`
checkout and exact `endpoint.cpp` SHA256. It checks the generated static
library symbol, isolated pkg-config libdir and lab executable before sending
traffic. One explicit operator-approved UDP Asterisk 1002->1001 call follows.
A real `Call::lookup()` -> callback override correlation and bounded pre-gate
pause are required; absent/contradictory markers fail closed.

The lab is **not CI** and requires explicit approval. Before using it, the
operator must understand that extension 1001 may ring and the lab attempts
exactly one call; it is not a teardown race injection. Run on the P8-R3C
branch from baseline `40cffc5` with a clean vendor Git checkout:

```bash
PHASE8_LIVE_SIP=1 PHASE8_R3C_CONFIRM=1002-to-1001 \
    bash scripts/phase8/run-r3c-vendor-dispatch-lab.sh
```

Enter the SIP password privately at the prompt (or supply
`VOIP_SIP_PASSWORD` in the environment); never include it in logs or Git.
The script stores a fresh allowlisted summary in `~/Downloads`, while raw
SIP output is deleted. Isolated build logs remain only under ignored `build/`.
A rerun refuses to reuse the build directory to avoid stale instrumented
libraries; manually archive/relocate it after investigating results. Once
(and only once) the sanitized `P8R3C_...` summary validates, add bounded
live evidence to the **same PR** with
`python3 -B scripts/phase8/record-r3c-evidence.py PATH_TO_SUMMARY`.
This creates a new file under `evidence/phase8` but does not commit it.
An inconclusive or failed run must not be recorded as PASS.

**Before a verified live run:** `REAL_VENDOR_PRE_GATE_PAUSE=NOT_RUN`;
external callback quiescence **UNPROVEN**, integration readiness **BLOCKED**,
Phase 8 acceptance **PENDING**. Even a live observation `PASS` establishes
only that bounded lookup-to-override dispatch happened, not a vendor-wide
lifetime fence or safe destruction of all C++ Call targets.

## Build and run

Aggregate offline verification:

```bash
bash scripts/phase8/verify.sh
```

Concurrency/stress verification:

```bash
bash scripts/phase8/verify-threading.sh core
THREADING_STRESS_REPEATS=100 bash scripts/phase8/verify-threading.sh stress
```

ThreadSanitizer uses the normal root CMake project in a separate build:

```bash
CXX="$(command -v clang++-18)" \
THREADING_TSAN_REPEATS=3 \
bash scripts/phase8/verify-threading.sh tsan
```

The production-client stop/reuse experiment also requires explicit operator
opt-in, Asterisk `192.168.1.23` with extensions `1002` and `1001`, and
PJPROJECT 2.17. It may ring `1001` and does not run under CI:

```bash
PHASE8_LIVE_SIP=1 PHASE8_R2A_CONFIRM=1002-to-1001 \
    bash scripts/phase8/run-production-stop-reuse-lab.sh
```

The script requests the SIP password without printing it and retains an
allowlisted summary only. Build output stays outside the Git archive under
`build/phase8-r2a-live/`. The bounded P8-R2A observations from 2026-10-12 are recorded in
`evidence/phase8/README.md`; their outcomes are not a general SIP call
delivery or PJSUA2 teardown guarantee.

P8-R2B is an additional **single-run, operator-approved** live experiment
for PJPROJECT 2.17. It deliberately attempts non-forced account shutdown
while a production outgoing call is still active. The target may ring;
there are no automatic retries. This is disabled in normal and CI builds:

```bash
PHASE8_LIVE_SIP=1 PHASE8_R2B_CONFIRM=1002-to-1001-active-shutdown \
    bash scripts/phase8/run-production-busy-recovery-lab.sh
```

The test fails closed if real `PJ_EBUSY`, retained Account/Endpoint, call
cleanup, production `stop()`, same-object restart and registration are not
all observed. It reports an explicit inconclusive result if active-call
preconditions disappear before the early attempt. A single P8-R2B lab run on
2026-10-12 observed real `PJ_EBUSY`, retained owners, disconnection, returned
production `stop()`, same-object restart, renewed registration and clean exit.
See `evidence/phase8/p8-r2b-live-pj-ebusy-20261012.txt`. This verifies only
the observed outgoing lab-only early-shutdown sequence; it does not prove
universal production shutdown safety or external callback quiescence.

The live lifecycle probe is explicit opt-in only:

```bash
PHASE8_LIVE_SIP=1 bash scripts/phase8/run-pjsip-live-lifecycle-probe.sh
```

## Verification

Recorded Phase 8 results include:

- canonical CTest: **147/147 PASS** on the recorded final gate;
- repeated 100,000-event MPMC stress: **PASS**;
- Clang 18 TSan: **18/18 PASS** in the recorded six-test gate;
- Qt/PJSIP ownership and shutdown source regressions: **PASS** within their offline scope;
- historical preview-mode manual GUI acceptance: **PASS**;
- controlled live PJSUA2 `PJ_EBUSY` recovery: **OBSERVED/EVIDENCED**.

The P8-R1 shutdown-attempt regression injects synthetic shutdown failures through
one helper now invoked by all three production clients. It verifies fail-closed
result translation and a modeled explicit retry; **it does not exercise real
PJSUA2 client recovery or establish external callback quiescence**. This new
test is additive to the previously recorded 147-test baseline.

Offline PASS does not upgrade the unresolved live production shutdown boundary.
The 2026-10-12 P8-R2A live observation recorded two **PASS** stop/reuse
cycles following an outgoing call attempt with response `PENDING`, and one
earlier **FAIL** after a rejected call response. The two PASS runs observed
explicit disconnect, same-object restart, renewed registration, second stop,
and normal destruction. They do not establish that extension `1001` received
or answered the call. The separate P8-R2B live run observed real `PJ_EBUSY` on
a production-owned outgoing Account and successful subsequent stop/restart in
its deliberately early-shutdown lab sequence; results for other production
clients and external callback quiescence remain **UNPROVEN**.

## Evidence

The concise evidence index is `evidence/phase8/README.md`.

Retained artifacts cover manual GUI acceptance, controlled live PJSUA2 lifecycle behavior, MPMC stress, and TSan. Commit-by-commit closure logs are intentionally left to Git history rather than duplicated in the current evidence tree.

Never commit SIP passwords, Digest response/nonce/cnonce material, SRTP inline keys, raw sensitive SIP traces, or protected packet captures.

### P8-R3A: offline callback ingress contract

A dedicated canonical CTest audit enumerates the nine known callback
overrides across `SipRegistrationClient`, `SipOutgoingCallClient`, and
`SipIncomingCallClient`. It checks gate-before-owner access, rejected-entry
return paths, shared `CallbackContext` ownership, destructor fail-closed
ordering, and Account retention through `libDestroy()`. A concurrent unit
regression confirms that a callback rejected after gate close remains counted
until its method returns. The audit intentionally fails if new callbacks are
added without review. **Neither test is a live PJSUA2 callback-source
quiescence proof.** Live vendor worker/ingress review remains for P8-R3B/C.

### P8-R3B: controlled lookup-to-gate lifetime model (offline only)

A canonical CTest models the precise window between a vendor `Call::lookup()`
and derived callback-gate entry. One deterministic schedule shows that a
closed/empty local gate does not protect a synthetic non-owning target in
that window; a separate schedule exercises a *test-only* per-call external
ingress fence and stops retirement until its in-flight ticket returns. The
model uses `weak_ptr::expired()` to avoid unsafe dereferences. It does not
use PJPROJECT, live SIP or production `TrackedCall` objects.

A second CTest audits all four known production member `call_.reset()` sites
and four temporary rejected `pj::Call` objects. These source checks are
fail-closed review triggers, not vendor synchronization barriers. Run:

```bash
ctest --preset qt-pjsip-gui-debug --output-on-failure \
    -R '^(PjsipLookupToGateLifetimeModelTest|Phase8R3BCallReleaseInventoryTest)$'
```

Evidence obtained from source review of PJPROJECT tag 2.17 identifies raw
Call pointer lookup, deferred media dispatch and vendor work before the
application gate. No instrumented vendor dispatcher, complete callback-source
stop/drain, or production UAF reproduction is claimed. R3B vendor-instrumented
verification and R3C live verification remain pending; external callback
quiescence stays **UNPROVEN**, PJSUA2 integration readiness **BLOCKED** and
Phase 8 acceptance **PENDING**.

## Interview focus

Be able to explain:

- queue close versus local stop-token cancellation;
- serial-worker ownership and drain semantics;
- what MPMC stress and TSan do and do not prove;
- why Qt queued delivery remains the production UI handoff;
- why call-slot count is not external callback quiescence;
- why the current PJSUA2 integration boundary remains fail-closed.

See `docs/interview/phase8-questions.md`.

## Exit criteria

| Criterion | State |
|---|---|
| Queue/worker implementation and deterministic tests | PASS |
| 100,000-event MPMC stress | PASS evidence |
| Clang 18 TSan | PASS evidence |
| Qt owner-thread handoff regression | PASS |
| Controlled PJSUA2 `PJ_EBUSY` recovery | OBSERVED/EVIDENCED |
| P8-R3A callback-target ingress source contract | OFFLINE TEST ONLY; external source still UNPROVEN |
| P8-R3B synthetic lookup-to-gate lifetime / release inventory | OFFLINE MODEL ONLY; vendor dispatch UNPROVEN |
| Live external callback quiescence | UNPROVEN |
| Production outgoing Account real `PJ_EBUSY` / lab-only early-shutdown recovery | OBSERVED/EVIDENCED (one bounded run; not normal `stop()` busy injection) |
| Other production-client `PJ_EBUSY` paths and external callback-source safety | UNPROVEN |
| PJSUA2 integration readiness | BLOCKED |
| Final maintainer acceptance | PENDING |
| Separate Phase 8 release tag | None defined |

Final acceptance must explicitly acknowledge the unresolved PJSUA2 boundary; merge history alone is not acceptance.

## References

- `ROADMAP.md`
- `docs/architecture/threading-model.md`
- `docs/interview/phase8-questions.md`
- `evidence/phase8/README.md`
- `tests/unit/`
- `tests/integration/`
- `scripts/phase8/`

## Next phase

Do not start Phase 9 until Phase 8 receives explicit maintainer acceptance. The roadmap assigns `v0.9-diagnostics` to Phase 9.
