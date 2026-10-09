# Phase 8 — Commit 12: Production PJSUA2 callback lifetime audit

**Baseline:** `phase/8-multithreading` at `f77d0b3` (Commit 11).
**Classification:** static source contract **PASS** / live callback quiescence **INCONCLUSIVE**.
**Scope:** `src/pjsip/SipOutgoingCallClient.cpp`, `src/pjsip/SipIncomingCallClient.cpp` (source from archived tree with Commit 9/10 overlays).
**Production changes:** none. Neither `CallbackActivityGate` nor a new shutdown sequence is wired into PJSUA2 by this commit.

## What the audit can observe

| Boundary | Outgoing | Incoming | Meaning |
|---|---|---|---|
| Account `onRegState()` | lines 20–31 | lines 20–31 | Callback borrows client reference; no dedicated callback-teardown mutex in this entry point. State updates later take `registration_mutex_`; this is not external callback admission control. |
| Account `onIncomingCall()` | lines 33–46 | lines 33–46 | Takes `incoming_callback_mutex_` before dispatch to owner. |
| Call `onCallState()` | lines 63–76 | lines 59–72 | Takes `call_callback_mutex_`; dispatches to owner. |
| Call `onCallMediaState()` | lines 78–91 | lines 74–87 | Takes `call_callback_mutex_`; may call audio/Endpoint functions. |
| Call slot wait | line 187 | line 176 | Wait occurs before `call_.reset()`, as hardened in Commit 9. A zero count is not proof that all callbacks returned. |
| Reset and account shutdown | lines 189–206 | lines 178–195 | `call_.reset()` precedes `shutdown_account_safely()`, `account_.reset()`, `libDestroy()`; the booleans from the call-slot wait and account shutdown are discarded. |
| Account shutdown helper | lines 452–471 | lines 355–374 | `shutdown2(force=false)` may fail and return `false` from wrapper. Caller does not branch on this result. |

Line references are to the **Commit 12 source baseline** with the current reviewed `Sip*CallClient.cpp` files. They are not claims about runtime callback scheduling.

## Open failure paths (not test failures)

1. **Call-slot timeout:** both `stop()` functions cast the result of `wait_for_call_cleanup(std::chrono::seconds{2})` to `void` and proceed to delete the derived `Call` object. A timeout therefore does not prevent teardown.
2. **Account `PJ_EBUSY`:** `shutdown_account_safely()` calls `shutdown2(force=false)` and returns `false` after an exception. Both `stop()` implementations ignore that result and execute `account_.reset()` anyway. PJSUA2's derived `Account` destruction requires careful shutdown ordering to avoid callbacks reaching a partly destroyed object.
3. **Borrowed owner:** derived `Call` and `Account` callbacks hold a `Sip*CallClient &owner_` reference. Locks guard selected paths but do not by themselves guarantee no *future* callback entry after object destruction.
4. **Registration callback:** `onRegState()` is outside the `incoming_callback_mutex_` / `call_callback_mutex_` regions. Its state lock is not a teardown/entry gate.
5. **Callback gate not wired:** Commit 11's `CallbackActivityGate` remains a separate validated model, not an adapter around actual PJSUA2 callback entry points. Importantly, adding such a gate without preventing *later* callback entry would not prove lifetime safety.
6. **Mutex reentrancy:** several callback handlers invoke PJSUA2 API while callback mutexes are held; verify whether these API calls synchronously trigger nested callbacks before changing lock acquisition order. No deadlock has been reproduced.

These are **static risks, not observed UAF, data races or deadlocks**. Do not treat the passing audit gate as a runtime safety guarantee.

## Files and verification gates

- `scripts/phase8/audit-pjsip-production-lifetime.py`: bounded inspection of actual callback class method bodies, mutex entry points, call-slot polling, stop ordering and non-forced account shutdown. JSON export available with `--json`; `--require-live-proof` deliberately exits nonzero because real callback quiescence has not been established.
- `tests/phase8/test_pjsip_production_lifetime_audit.py`: nine regression tests including mutations that reorder teardown, remove callback locks, remove the real call-slot probe, force account shutdown or remove an override.
- `scripts/phase8/verify-pjsip-production-lifetime-audit.sh`: executes the source audit, nine mutation tests and the new CTest once in each focused/canonical registration.
- `tests/phase8/CMakeLists.txt`: registers `Phase8PjsipProductionAuditTest` as the **142nd canonical CTest** and **7th focused CTest**. The Python audit is intentionally omitted from standalone ThreadSanitizer builds, which continue exercising six instrumented C++ tests (18 executions at three repetitions).
- `evidence/phase8/pjsip-production-lifetime-audit.txt`: prepared findings; replace `PENDING` only after inspecting real ThinkPad output. No SIP secrets or PCAPs are required.

### ThinkPad acceptance

```bash
bash scripts/phase8/verify-pjsip-production-lifetime-audit.sh
bash scripts/phase8/verify-queue.sh
CXX="$(command -v clang++-18)" PHASE8_TSAN_REPEATS=3 \
  bash scripts/phase8/verify-tsan.sh
bash scripts/phase8/verify-pjsip-shutdown-order.sh
bash scripts/phase8/verify-qt-ownership.sh
cmake --build build/qt-pjsip-gui-debug -j"$(nproc)"
ctest --test-dir build/qt-pjsip-gui-debug --output-on-failure
```

Expected if unchanged and all tests pass: mutation tests 9/9, new targeted CTest 1/1 per build, focused **7/7**, existing PJSIP **45/45**, Qt ownership 1/1, isolated C++ TSan **6 × 3**, canonical **142/142**. No live SIP callback was exercised by these gates.

## Next step: runtime decision gate (outside Commit 12)

Before modifying `stop()`, construct a controlled SIP endpoint/account test with explicit signals for: pending INVITE, active call, callback active during shutdown, slot timeout, `shutdown2(force=false)` returning `PJ_EBUSY`, asynchronous registration callbacks, and callback-admission after shutdown initiation. A proposed fix must prove that `Call`, derived `Account`, owner and Endpoint outlive all callbacks that can reach them **without introducing deadlocks**. A model gate alone cannot establish this property.

### PJSIP API cross-check

- PJSUA2 `Account::shutdown2`: https://docs.pjsip.org/en/2.17/api/generated/pjsip/group/group__PJSUA2__ACC.html
- PJSUA2 call disconnection and lifetime: https://docs.pjsip.org/en/2.17/pjsua2/using/call.html
- PJSUA2 `Endpoint::libDestroy`: https://docs.pjsip.org/en/2.17/api/generated/pjsip/group/group__PJSUA2__UA.html

The audit script examines source structure rather than executing these external APIs.


## ThinkPad verification — 2026-10-09T23:17:06+07:00

Verified against pre-commit HEAD `f77d0b3`:

- Production source audit: PASS.
- Mutation tests: 9/9 PASS.
- Focused Phase 8 CTests: 7/7 PASS.
- Clang 18 ThreadSanitizer: 18/18 executions PASS.
- PJSIP CTests: 45/45 PASS.
- Qt ownership regression: PASS.
- Full canonical CTests: 142/142 PASS.

**Conclusion:** The source audit and automated regression
gates passed. Live PJSUA2 callback quiescence remains
**INCONCLUSIVE**.

This commit does not resolve the six open production
shutdown/lifetime risks identified above.
