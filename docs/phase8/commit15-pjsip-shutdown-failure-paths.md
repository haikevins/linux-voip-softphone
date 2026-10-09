# Phase 8 — Commit 15: PJSIP shutdown failure-path regression model

**Baseline:** `phase/8-multithreading`, expected HEAD `a9ba51f` (Commit 14).
**Change class:** regression tests, verification script, docs/evidence; **no production source changes**.
**Outcome sought:** deterministic proof that a *test-only candidate policy* stays fail-closed when call slots, account shutdown, or callback lifetime guarantees are unavailable. This is **not** a proof that the production SIP clients use that policy.

## Why a model instead of modifying live shutdown

Both `SipOutgoingCallClient::stop()` and `SipIncomingCallClient::stop()` currently discard the boolean from `wait_for_call_cleanup(2s)` and the boolean from `shutdown_account_safely()`, then reset `call_` and `account_` regardless of failure. On `PJ_EBUSY` or call-slot timeout the derived PJSUA2 callback targets may still be reachable. Their registration callbacks also lack external callback-admission control. The Commit 12 static lifetime audit reported these unresolved paths; Commit 15 **does not fix them**.

Changing destruction order or adding a gate to the real clients before verifying the library's callback/thread quiescence could create use-after-free or deadlocks. A closed local `CallbackActivityGate` proves only that callbacks *admitted through that gate* have exited. It does not prove PJSUA2 has stopped entering callbacks.

## Added model coverage

`tests/phase8/test_pjsip_shutdown_failure_paths.cpp` exercises the real `detail::wait_for_zero_call_slots` polling helper and `detail::CallbackActivityGate` in a synthetic teardown decision harness. The harness retains simulated `Call`, `Account`, and `Endpoint` owner flags unless **all four** independent conditions hold: call-slot drain, successful account shutdown, closed/empty admitted-callback gate, **and a separately supplied external no-more-callbacks guarantee**.

| Scenario | Required model behavior |
| --- | --- |
| Call slots permanently busy past 25 ms deadline | Timeout is observed; all owners retained |
| Simulated `PJ_EBUSY` / false account shutdown | All owners retained |
| Injected account shutdown exception | All owners retained |
| Zero call slots but admitted callback still active | All owners retained until permit released |
| Callback gate still open | All owners retained |
| Gate closed/empty but external callback stop unconfirmed | All owners retained |
| Combined failures followed by partial resolution | All owners retained until *all* preconditions hold |
| Explicit synthetic success | Release simulated `Call` → `Account` → `Endpoint` exactly once |
| Failed shutdown followed by fully authorized retry | No partial release; retry succeeds only with all signals |

`external_callback_source_stopped` is a test-supplied Boolean, **not** a runtime property established by this project. The modeled success cases are *not* evidence that production can safely set it to true.

## Existing production source inventory is still open

`scripts/phase8/verify-pjsip-shutdown-failure-paths.sh` uses the Commit 12 source auditor to ensure both real SIP client bodies are still recognizable and report the six previously documented open risks: discarded call-slot result, discarded account shutdown result, possible account destruction after failure, no registration callback teardown gate, no wired production callback gate, and borrowed owner references. This is a bounded risk **inventory**, not an assertion that an unsafe condition is fixed. If future production code intentionally removes a risk, update the inventory and its evidence in a separately reviewed commit rather than silently assuming its safety.

The script then builds and runs the new model test in both focused and canonical CTest configurations. The new model test is **excluded** when `PHASE8_ENABLE_TSAN=ON` so the existing six-test × three-runs isolated TSan gate remains unchanged.

Expected counts if the baseline remains unchanged: focused **8/8**, canonical **143/143**, PJSIP **45/45**, isolated Clang18 TSan **6 C++ tests × 3 runs**. The new test target does not link PJSUA2.

## ThinkPad acceptance (must be observed before commit)

1. Verify HEAD = `a9ba51f`, branch `phase/8-multithreading`, clean tree and origin parity; verify exact overlay SHA256 and three baseline source hashes.
2. Verify full `clang-format --dry-run --Werror` on `src`, `tests`, `labs` and the full `clang-tidy` preset build; neither check may be relaxed or suppressed.
3. Run `bash scripts/phase8/verify-pjsip-shutdown-failure-paths.sh` and inspect both focused and canonical targeted CTest runs plus the **OPEN** source-risk inventory.
4. Run `bash scripts/phase8/verify-queue.sh`, Qt ownership regression, PJSIP shutdown-order regression, Clang18 isolated TSan ×3, canonical build and full canonical CTest.
5. Record **actual observed ThinkPad results** in `evidence/phase8/pjsip-shutdown-failure-paths.txt`, stage only the ZIP manifest, commit/push and verify remote parity + clean tree. GitHub Actions is an independent post-push gate.

Recommended targeted verification command:

```bash
bash scripts/phase8/verify-pjsip-shutdown-failure-paths.sh
```

**Not proven:** real PJSUA2 account `PJ_EBUSY` recovery, timeout survival under active SIP traffic, external callback quiescence, late callback admission, reentrancy and deadlock freedom. These require a controlled SIP integration/fault-injection environment before modifying production lifetime management.


## ThinkPad verification — Commit 15

**THINKPAD VERIFIED PHASE 8 COMMIT 15**

Recorded: 2026-10-10T00:24:34+07:00

- Branch: `phase/8-multithreading`.
- Baseline HEAD: `a9ba51f`.
- Full clang-format 18.1.3: PASS.
- Full clang-tidy 18.1.3: PASS.
- Focused + canonical failure-path model targets: PASS.
- Production source risk inventory: PASS; risks remain OPEN.
- Focused Phase 8 CTest: 8/8 PASS.
- Clang 18 ThreadSanitizer: 18/18 PASS.
- Qt ownership regression: PASS.
- PJSIP regression: 45/45 PASS.
- Canonical configure/build and CTest: 143/143 PASS.
- GitHub Actions for the new commit: PENDING.

These are observed automated ThinkPad results.
Production SIP shutdown safety and live PJSUA2 callback
quiescence are NOT established by this commit.
