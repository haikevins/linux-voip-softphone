# Phase 8 Commit 11 — Callback activity is not PJSIP quiescence

**Proposed baseline:** `phase/8-multithreading` at `8bef4c7` (Commit 10).
**Status:** ThinkPad automated verification **PASS**; Git commit **pending**. Live SIP callback verification remains pending.

## Motivation and source-backed risk

Commit 10's `wait_for_zero_call_slots` reports a *momentary call-slot
observation*. It cannot certify callback return, or prevent a library worker
from invoking a callback later. The outgoing and incoming call clients define
PJSUA2 `Account::onRegState`, `Account::onIncomingCall`,
`Call::onCallState`, and `Call::onCallMediaState` overrides. Their `stop()`
paths may still proceed with account and call object destruction when a 2s
call-slot wait expires. `shutdown_account_safely()` returns a bool which
current callers ignore; `shutdown2(force=false)` may fail with active calls.

PJSIP 2.17 documents that callbacks may originate on its worker threads:
<https://docs.pjsip.org/en/2.17/api/generated/pjsip/group/group__PJSUA2__UA.html>.
Account destructor/shutdown rules and `shutdown2` busy behavior:
<https://docs.pjsip.org/en/latest/api/generated/pjsip/group/group__PJSUA2__ACC.html>.

## This commit's deliberately narrow scope

- Adds `src/pjsip/CallbackActivityGate.hpp`: an **unwired** C++20 proof-of-
  concept with move-only RAII callback permits, stop-admission semantics, and
  bounded wait for *already admitted* work.
- Adds deterministic tests for seven scenarios including a zero-slot
  observation with an active callback, deadline timeout that **never grants
  cleanup permission**, move-only permit accounting, concurrent drain, and
  race between admission and closing.
- Registers one new standalone and canonical CTest
  `Phase8PjsipCallbackActivityTest`. Counts expected if baseline holds:
  **focused 6/6**, **canonical 141/141**, Clang 18 TSan **six tests x3 =18
  executions**. These numbers require ThinkPad verification before claiming
  PASS.
- Adds `scripts/phase8/verify-callback-activity.sh` to build/run just this
  CTest twice: in the independent Phase 8 tree and in canonical parent.
- No production PJSIP or GUI callback hook is modified in Commit 11.

## Critically important limitation

`CallbackActivityGate::closed_and_empty()` is **not** proof that the C++
object owning it can be safely destroyed. It only describes callbacks that
have **already acquired** permits. A new PJSIP worker callback that starts
**after** object destruction would dereference invalid memory before it could
acquire a permit. A separately established external callback-source shutdown /
lifetime contract is therefore mandatory before any production integration.
Do not use this gate alone to 'fix' the 2-second timeout or claim race freedom.

## Acceptance

On maintainer ThinkPad, at clean Git baseline `8bef4c7`, apply overlay only
after SHA256/manifest/baseline checks. Run these gates, record transcripts in
`/tmp` and **only then** append to the new evidence file:

```bash
bash scripts/phase8/verify-callback-activity.sh
bash scripts/phase8/verify-pjsip-shutdown-order.sh
bash scripts/phase8/verify-qt-ownership.sh
bash scripts/phase8/verify-queue.sh
CXX="$(command -v clang++-18)" \
PHASE8_TSAN_BUILD_DIR="$PWD/build/phase8-tsan-clang18-debug" \
PHASE8_TSAN_REPEATS=3 bash scripts/phase8/verify-tsan.sh
cmake -S . -B build/qt-pjsip-gui-debug
cmake --build build/qt-pjsip-gui-debug --parallel 2
ctest --test-dir build/qt-pjsip-gui-debug --output-on-failure
```

Only after focused 6/6, TSan 18 observed executions, and canonical 141/141
PASS should these be recorded as actual ThinkPad evidence. This is not a live
SIP or production callback-lifetime test.

## Unresolved for next integration slice

Before wiring any gate to PJSUA2 callbacks, obtain and verify the library's
callback-source quiescence behavior, including active registration callbacks,
late new INVITEs, active media callback/teardown, and account `PJ_EBUSY`
shutdown. A real SIP account/test server is required for runtime acceptance.


## ThinkPad automated verification — 2026-10-09T23:01:25+07:00

Verified on `phase/8-multithreading` at pre-commit HEAD `8bef4c7`:

- Standalone and canonical callback-model CTests: PASS.
- PJSIP shutdown structural tests: 6/6 PASS.
- PJSIP CTests: 45/45 PASS.
- Qt ownership regression: PASS.
- Focused Phase 8 CTests: 6/6 PASS.
- Clang 18 ThreadSanitizer: 18/18 executions PASS.
- Full canonical CTest: 141/141 PASS.

No production SIP callback was exercised or instrumented.
These results do not establish external PJSUA2 callback quiescence.
