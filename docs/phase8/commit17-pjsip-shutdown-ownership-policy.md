# Phase 8 — Commit 17: shutdown ownership policy and stale-result regression

**Baseline:** `phase/8-multithreading`, exact HEAD `f56ba42` (Commit 16).

**Scope:** one modified `tests/phase8/CMakeLists.txt`, one new C++ policy test, one new executable verification script, this document, and one evidence file. **Production SIP/PJSUA2 implementation is unchanged.**

## Why this commit

Commit 15 modeled fail-closed resource ownership when slot cleanup or account shutdown fails. Commit 16 separated a simulated external callback source from the local gate and exercised ingress races. Neither guarantees that an asynchronous success or failure received **after a retry** belongs to the current shutdown attempt. Combining fresh and stale results could incorrectly authorize releasing `Call`, `Account` or `Endpoint`.

Commit 17 introduces a **test-only candidate policy** with monotonic attempt generations and four individually reported preconditions: (1) call slots drained, (2) account shutdown succeeded, (3) locally admitted callbacks drained, and (4) externally stopped/drained callback ingress. No production code supplies the fourth confirmation today. A `CallbackActivityGate::closed_and_empty()` check is not external PJSUA2 quiescence.

## Policy contract under test

- A fresh attempt owns a new generation and has **zero verified conditions**. Every release decision rejects missing evidence.
- A reported timeout, `PJ_EBUSY`-like busy status, exception, or missing callback quiescence terminates that attempt without releasing any modeled owner.
- Once an attempt has failed, late success cannot amend it; an explicit retry creates a new generation and requires all four confirmations again.
- Any report from an old generation is rejected, even if it arrives during an active retry. Receipts from different generations cannot be combined.
- Successful release is permitted only with all four verified receipts of **one current generation**. The simulated release order is `call` -> `account` -> `endpoint`, exactly once; release cannot be retried after success.
- A deterministic `std::latch` schedule delays a synthetic external callback receipt across a retry boundary and verifies that it cannot authorize release.

The test `tests/phase8/test_pjsip_shutdown_ownership_policy.cpp` exercises ten named scenarios, a 16-case failure grid (four error values by four checks), four missing-proof conditions, all 24 receipt orders, and a delayed callback-generation race. It uses the real `detail::CallbackActivityGate` only to demonstrate that local drain does not substitute for external callback proof. It does **not** link to PJSUA2 or create real `Call`, `Account`, or `Endpoint` instances. Its modeled `Call`/`Account`/`Endpoint` are owner flags and a release log, not an actual safe production ownership design.

## Important limits

This model has an explicit `begin_attempt()` retry. The production `SipIncomingCallClient::stop()` and `SipOutgoingCallClient::stop()` do **not** implement it. The source audit still reports unresolved discarded cleanup and account-shutdown results, a possible `PJ_EBUSY` lifetime hazard, borrowed owner references and missing callback ingress fencing. No test result here establishes runtime PJSUA2 callback quiescence, registration callback safety, a safe lifetime escape from an object destructor, or an actual recovery from `PJ_EBUSY`. None of those production hazards are fixed by this commit.

## Verification and expected counts

The new target is registered only when `PHASE8_ENABLE_TSAN` is OFF to preserve the existing isolated **six C++ tests × three runs** TSan contract. Expected standalone focused count after this change: **10/10**; full canonical CTest: **145/145**; existing PJSIP CTest: **45/45**; existing Qt owner-thread regression unchanged.

Run `bash scripts/phase8/verify-pjsip-shutdown-ownership-policy.sh` after configuring the canonical build; it compiles and runs the new model in both focused and canonical builds. Also run full CI-equivalent `clang-format` and `clang-tidy` before broader CTest. Re-run Commit 15 shutdown failure-path and Commit 16 callback ingress tests, isolated Clang18 TSan, Qt, PJSIP, focused suite and canonical build/CTest. Record only **observed ThinkPad results** in the evidence file, stage exactly five files, commit/push and verify remote HEAD parity and a clean working tree. Check GitHub Actions separately.

**Expected commit subject:** `test(phase8): model shutdown ownership retries and stale callbacks`.

## Commit 17 v2 formatting correction (ThinkPad C1)

On the maintainer ThinkPad, Commit 17 v1 passed baseline verification and exact five-file overlay application, but the first CI-equivalent clang-format gate reported four formatting diagnostics at lines 153–154 and 194–195 of the new policy test. Commit 17 v2 only repacks those two `std::array` initializer lists without changing test behavior or production code. The v2 clang-format gate was observed **PASS**, but full clang-tidy 18.1.3 failed with **38 warnings treated as errors** (three `performance-enum-size`, 35 `bugprone-unchecked-optional-access`). The remaining v2 regression gates were not run. Do not claim v1 successfully passed those gates. GitHub Actions is also PENDING.


## Commit 17 v3: clang-tidy regression correction (ThinkPad C2)

Commit 17 v3 addresses the *observed* 38 clang-tidy failures without weakening
`.clang-tidy` or changing production code:

- The three small enums use explicit `std::uint8_t` underlying types. Enum
  values, check identities and per-attempt policy decisions are unchanged.
- The test-only `begin_required_attempt()` unwraps an optional generation via
  `value_or(0)` and rejects zero. The model's generation counter starts at 1;
  a missing attempt is a test failure. The 35 downstream accesses use the
  validated generation integer, not unchecked optional dereferences.
- The model still obtains its generations from the *same* `begin_attempt()`
  API, retains all retry/stale-result checks, and preserves the 24-order grid
  and deterministic delayed-callback race.

**ThinkPad observation before v3:** v2 clang-format PASS; v2 clang-tidy FAIL
(38 errors). The v2 ZIP disappearing from `~/Downloads` on an accidental
repeated overlay command did not undo v2's already-applied source; verify its
current hashes before applying v3.

**ThinkPad v3 verification:** full clang-format, clang-tidy, and targeted
policy/failure-path/ingress model gates PASS. Focused CTest 10/10, Clang18
TSan 18/18, Qt ownership, PJSIP CTest 45/45, and canonical CTest 145/145
PASS. GitHub Actions for the eventual new commit remains PENDING.
This is still a synthetic test-only model, not evidence of PJSUA2 callback
quiescence or safe production ownership release.


## THINKPAD VERIFIED COMMIT 17 V3

Recorded: 2026-10-10T07:48:27+07:00

Branch: `phase/8-multithreading`
Baseline HEAD: `f56ba42`

Observed maintainer ThinkPad results:

- Full clang-format 18.1.3: PASS.
- Full clang-tidy 18.1.3: PASS.
- Commit 17 ownership policy model: PASS.
- Commit 15 shutdown failure-path model: PASS.
- Commit 16 callback ingress model: PASS.
- Focused Phase 8 CTest: 10/10 PASS.
- Clang 18 ThreadSanitizer: 18/18 PASS.
- Qt ownership regression: PASS.
- PJSIP regression: 45/45 PASS.
- Canonical configure/build and CTest: 145/145 PASS.
- Exact manifest: 1 modified + 4 new files.
- GitHub Actions for the new commit: PENDING.

These are automated verification results. The tests
do not prove live PJSUA2 callback quiescence,
PJ_EBUSY recovery, or production teardown safety.
