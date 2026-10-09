# Phase 8 — Commit 16: external callback ingress race model

**Baseline:** `phase/8-multithreading`, immediately after Commit 15, `test(phase8): cover PJSIP shutdown failure paths`; its final SHA must be confirmed on the maintainer ThinkPad. Parent of Commit 15 must be `a9ba51f` (Commit 14).

**Scope:** 1 change to `tests/phase8/CMakeLists.txt`; 1 new standalone C++ test; 1 executable verification script; this documentation and 1 evidence file. **No production SIP code or PJSUA2 internals changed.**

## Motivation and boundaries

Commits 11 and 15 introduced a `detail::CallbackActivityGate` and a synthetic shutdown fail-closed decision. A closed/empty gate observes only callbacks that successfully entered that gate: it does *not* stop PJSUA2 from issuing further callbacks and it cannot account for an external callback delayed just before attempting gate admission. A proposed production teardown must prove externally enforced callback quiescence before it destroys callback targets; the present project does not have such a guarantee.

Commit 16 adds `SyntheticExternalCallbackSource`, an independently synchronized *test-only* callback producer with `stop_admitting()` and RAII in-flight callback tickets. `source.quiesced()` becomes true only after admission has stopped **and** all tickets have exited. The tested model allows release only when call slots drained, account shutdown succeeded, the local gate is closed/empty, and the synthetic external source has stopped/drained. The fake source is **not** a PJSUA2 implementation or evidence that `libDestroy()` provides this guarantee in any timing window.

## Nine scenarios

1. The local gate closes and empties; external callback entry still succeeds until its source is stopped.
2. Stopping external callback admission does not imply already-running external callbacks have returned.
3. External source is stopped/drained while the local callback gate is open; release remains forbidden.
4. A callback already admitted by both boundaries blocks release even after external callback tickets exit.
5. Exhaustively check the `2^4 = 16` combinations of the independent release preconditions; only all true succeeds.
6. A threaded callback crosses the external boundary before gate close, is denied local admission afterward, and still blocks release until it exits.
7. A threaded callback admitted through both boundaries blocks release across simultaneous gate close and external stop, until it exits both.
8. Call-slot timeout or account shutdown failure blocks release despite both callback drains; synthetic retry succeeds once both recover.
9. Repeated external-stop and gate-close calls plus moving a callback ticket do not prematurely report source quiescence.

Tests use explicit `std::latch` synchronization rather than sleeps. The two worker race tests must not leak detached threads. These deterministic *model* schedules cannot cover unknown PJSUA2 internal callbacks or prove real callback ordering.

The target is included in normal standalone and canonical CTest when TSan is OFF. It is deliberately excluded from the fixed six-C++-test × three-run isolated Clang18 TSan gate, matching Commit 15's scope. Expected tests after the addition: focused **9/9**; canonical **144/144**; existing PJSIP **45/45**; Clang18 TSan **18/18 executions**.

## ThinkPad acceptance

1. Verify that the expected Commit 15 branch HEAD is clean and equals `origin/phase/8-multithreading`, that its subject is `test(phase8): cover PJSIP shutdown failure paths`, and that `HEAD^` is `a9ba51f`. Verify the baseline SHA of `tests/phase8/CMakeLists.txt` before overlay.
2. Verify the exact five-entry ZIP manifest, ZIP SHA256 and initial file nonexistence. Apply only this ZIP via checksum overlay and verify exact byte matches.
3. Run full CI-equivalent clang-format and clang-tidy gates; neither may be suppressed.
4. Run `bash scripts/phase8/verify-pjsip-callback-ingress-races.sh` (one focused and one canonical test), `bash scripts/phase8/verify-queue.sh` (9/9), `bash scripts/phase8/verify-pjsip-shutdown-failure-paths.sh` (models), Qt ownership, PJSIP shutdown order (45/45), isolated Clang18 TSan 6 × 3, and canonical build/CTest (144/144).
5. Record the **actual observed** ThinkPad results in `evidence/phase8/pjsip-callback-ingress-races.txt`, stage exact ZIP manifest, push feature branch and verify remote parity/clean tree. GitHub Actions must be checked separately.

**Unresolved production risk:** real account `PJ_EBUSY`, call-slot timeout with a live PJSUA2 callback, callback-after-stop, registration callback admission, callback-owner borrowed references, and late entry outside this model. Commit 16 does **not** repair any live-SIP shutdown path or prove callback quiescence. A controlled live PJSUA2 fault-injection test is required before changing production lifetime ownership.

## v2 formatting correction (pre-commit)

The original Commit 16 ZIP failed the maintainer ThinkPad clang-format 18.1.3
CI gate at `test_pjsip_callback_ingress_races.cpp:112:69`. The corrected
v2 overlay packs the first two function parameters into a 100-column line
as required by the repository style. **No test logic changes.** The corrected v2 overlay has now passed full clang-format, clang-tidy, and regression gates on the maintainer ThinkPad. GitHub Actions verification for Commit 16 remains PENDING.


## THINKPAD VERIFIED PHASE 8 COMMIT 16 V2

Recorded: 2026-10-10T00:47:44+07:00

Branch: `phase/8-multithreading`
Baseline HEAD: `7ca3247` (Commit 15)

Observed ThinkPad verification:

- Full clang-format 18.1.3: PASS.
- Full clang-tidy 18.1.3: PASS.
- Synthetic callback ingress model: PASS.
- Shutdown failure-path model: PASS.
- Focused Phase 8 CTest: 9/9 PASS.
- Clang 18 ThreadSanitizer: 18/18 PASS.
- Qt ownership regression: PASS.
- PJSIP regression: 45/45 PASS.
- Canonical configure/build and CTest: 144/144 PASS.
- Exact source manifest: 1 modified + 4 added.
- GitHub Actions for the new commit: PENDING.

These results validate test-only concurrency models and existing
automated regression gates. They do not establish live PJSUA2
callback quiescence, real PJ_EBUSY recovery, or production
shutdown lifetime safety.
