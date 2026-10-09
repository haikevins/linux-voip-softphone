# Phase 8 — Commit 10: deterministic PJSIP call-slot polling

**Baseline:** `phase/8-multithreading @ 368f510` (after Commit 9).
**Scope:** share and directly test the bounded polling policy used by outgoing
and incoming `wait_for_call_cleanup()`. No change to `stop()` teardown order,
account ownership, PJSUA2 callback registration, or timeout fallback.

## Motivation and boundary

Commit 9 made `stop()` wait up to two seconds before `call_.reset()` for both
SIP call clients. The existing six Python source-contract tests verify ordering,
but not the control flow inside the busy-wait loop. `pjsua_call_get_count()`
returning zero reports a momentary call-slot observation. It is **not** proof
that a callback on another thread has exited.

Current code also attempts `shutdown_account_safely()` then unconditionally
resets the account. PJSUA2 2.17 `Account::shutdown2(force=false)` may throw
`PJ_EBUSY` when calls remain. These cases are unresolved risks; this commit
does not assert that a failed account shutdown or a two-second timeout is safe.

## Changes

- `src/pjsip/PjsipCallSlotCleanup.hpp` provides a small C++20 dependency-free
  polling function. The production clients call it using the real
  `pjsua_call_get_count`, `steady_clock::now`, and `sleep_for(10ms)`. Failure
  logging and the surrounding teardown order remain in their existing paths.
- `tests/phase8/test_pjsip_call_slot_cleanup.cpp` runs six deterministic
  scenarios using a synthetic clock; no SIP server, credential, wall-clock
  delay, or Qt dependency is needed.
- A fifth Phase 8 CTest (`Phase8PjsipCallSlotCleanupTest`) is registered both
  standalone and under canonical CTest. Expected canonical count **140**;
  standalone count **5**. Standalone TSan also covers five tests.
- `scripts/phase8/verify-pjsip-cleanup-timeout.sh` rebuilds affected PJSIP
  targets, runs the new policy CTest and the existing 45 PJSIP CTests.
  This is a build/behavior gate, **not a live SIP callback-lifetime test**.

## Test cases

1. Call count starts at zero: return success without sleep.
2. Busy count becomes zero after several polls: return success.
3. Count stays busy through the deadline: report timeout (false).
4. Zero duration with a busy count: report timeout without sleep.
5. Count reaches zero precisely at the deadline: count probe takes priority.
6. A zero count followed by a later nonzero count illustrates that a probe
   is not a callback-quiescence or future-activity barrier.

## Verification commands (ThinkPad)

```bash
bash scripts/phase8/verify-pjsip-cleanup-timeout.sh
bash scripts/phase8/verify-pjsip-shutdown-order.sh
bash scripts/phase8/verify-qt-ownership.sh
bash scripts/phase8/verify-queue.sh
CXX="$(command -v clang++-18)" \
  PHASE8_TSAN_BUILD_DIR="$PWD/build/phase8-tsan-clang18-debug" \
  PHASE8_TSAN_REPEATS=3 bash scripts/phase8/verify-tsan.sh
cmake --build build/qt-pjsip-gui-debug -j"$(nproc)"
ctest --test-dir build/qt-pjsip-gui-debug --output-on-failure
```

Required pre-commit results: six deterministic cases, one canonical policy
CTest, **45/45 PJSIP CTests**, **5/5 Phase 8 CTests**, five TSan tests x three
with Clang 18, Qt ownership gate, and **140/140 canonical CTests**. Do not
record PASS for any gate without maintainer-provided output.

## Out of scope / not yet proven

- A genuine active INVITE, incoming ringing, active RTP, or network failure.
- Race between `pjsua_call_get_count()` and a late PJSUA2 callback.
- In-flight callback when `call_.reset()` or `account_.reset()` is executed.
- `shutdown2(force=false)` reporting busy, failed account shutdown, or
  forced endpoint teardown after a two-second timeout.
- Application-wide TSan instrumentation (only standalone Phase 8 code is
  instrumented); no proof of absence of data races or use-after-free.

Next phase work should use a controlled SIP test endpoint and fault-injected
callback timing before changing timeout fallback or claiming app-wide safety.
