# Phase 8 — Commit 18: production-facing PJSIP shutdown authorization

**Required baseline:** `phase/8-multithreading` at Commit 17, `b1071348fb3beff1adf6a078fd153d340ea93201`.

**Commit subject:** `feat(phase8): introduce PJSIP shutdown authorization helper`.

## Motivation and scope

Commits 15–17 tested synthetic failure paths, callback ingress, and epoch-bound ownership policy. Commit 18 introduces a **real reusable, header-only C++20 bookkeeping component** in `src/pjsip/PjsipShutdownAuthorization.hpp`. The new focused/canonical CTest target exercises **this production source header directly**. This is an integration prerequisite, **not** a change to either live `Sip*CallClient::stop()` or a proof of teardown safety.

Exact overlay: one modified `tests/phase8/CMakeLists.txt` and five added files: a production header, a C++ regression, an executable verification script, this document, and one evidence file. No existing production call site, PJSUA2 account/call subclass, Qt backend, or linter rule changes.

## Authorization contract

`voip::pjsip::detail::PjsipShutdownAuthorization` is mutex-protected and tracks a monotonic shutdown generation. A new attempt clears all previous receipts. It accepts four *separate* confirmations of the current generation:

1. `CallSlotsDrained` — the real PJSIP call-slot count reached zero; **not** a callback lifetime guarantee.
2. `AccountShutdownSucceeded` — the account shutdown operation reported success; a thrown error or `PJ_EBUSY`-like failure is **not** success.
3. `LocalCallbacksDrained` — callbacks admitted to the local gate are complete.
4. `ExternalCallbackIngressQuiescent` — an independently established guarantee that the **external PJSUA2 source will not enter or continue callbacks**.

The helper does **not** generate any of these receipts itself. The fourth proof has **no verified provider in current production**, and **must never** be derived merely from `CallbackActivityGate::closed_and_empty()` or a zero call-slot count. `authorize(generation)` only changes the helper's internal bookkeeping state; it does not destroy, preserve, or own `Call`, `Account`, or `Endpoint`. It must **not** be wired into `stop()` until a genuine external callback-quiescence contract, ownership escape strategy for `noexcept` destructors, and `PJ_EBUSY` recovery are designed and tested.

Rules: missing/invalid/duplicate proof cannot authorize; timeout/busy/exception/nonquiescence is terminal for the current attempt; an explicit retry invalidates all prior receipts; late success or failure from older generations is rejected; four valid confirmations in **one** generation may authorize exactly once; authorization cannot be reopened.

## Regression coverage

`tests/phase8/test_pjsip_shutdown_authorization.cpp` validates missing proof (4 cases), four failure kinds × four proof categories (16 cases), duplicate/invalid proofs, stale success/failure across retry, a deterministic `std::latch` late-callback schedule, fresh retry after busy, and every permutation of four successful confirmations (24 orders). The tests are deterministic and use no SIP network, credentials, or PJSUA2 instance. The existing Commit 17 model remains unchanged to facilitate cross-checks.

The new CTest target is added only under `NOT PHASE8_ENABLE_TSAN`, maintaining the isolated Clang18 TSan inventory at **six existing C++ tests × three runs = 18 executions**. Expected Phase 8 focused CTest: **11/11**. Expected canonical CTest: **146/146**. Existing PJSIP regression: **45/45**.

## Maintainer ThinkPad validation (PRE-COMMIT VERIFIED)

1. Check baseline HEAD, origin parity and clean tree; verify ZIP SHA256, exact manifest and source hashes before applying via `rsync --no-times --checksum`.
2. Run full CI-equivalent `clang-format` 18 and `clang-tidy` 18 first; stop at any diagnostic.
3. Run `scripts/phase8/verify-pjsip-shutdown-authorization.sh`, focused 11/11, Clang18 isolated TSan 18/18, Qt, PJSIP 45/45, and canonical CTest 146/146.
4. Record **actual ThinkPad logs**, update evidence, stage exactly six files, commit and push. Verify remote HEAD parity, clean tree, and GitHub Actions separately.

### Safety boundary

**Production shutdown is still known to be unsafe/unverified on failure paths**: both `stop()` methods currently discard `wait_for_call_cleanup()` and `shutdown_account_safely()` results before unconditional resource reset, and registration/call callbacks borrow client owners by reference. No live PJSUA2 callback-quiescence proof, `PJ_EBUSY` recovery, or destructor-safe ownership escape has been established. A passing CI suite for this commit must not be cited as proof those problems are resolved.

## v2 clang-format correction (ThinkPad C1)

The initial Commit 18 overlay passed baseline/overlay application but **failed**
ThinkPad clang-format 18.1.3 at two formatting-only sites in the new
`test_pjsip_shutdown_authorization.cpp` (the `proofs` initializer and the
late-receipt `require` expression). v2 uses an explicit trailing-comma
initializer and a named `call_slots` enum value to make the late-receipt
expression fit on one line without altering test behavior. No header,
CMake, script, or production teardown change was made. v2 must be rechecked
with the actual ThinkPad clang-format and clang-tidy gates; all other gates
remain **PENDING** until maintainer logs are observed. The earlier C1 failure
must not be reported as a pass.


## THINKPAD VERIFIED PHASE 8 COMMIT 18 V2

Recorded: 2026-10-10T08:05:07+07:00

Branch: `phase/8-multithreading`
Baseline HEAD: `b1071348fb3beff1adf6a078fd153d340ea93201`

Observed maintainer ThinkPad results:

- Full clang-format 18.1.3: PASS.
- Full clang-tidy 18.1.3: PASS.
- Production-header shutdown authorization regression: PASS.
- Commit 17 ownership policy model: PASS.
- Commit 15 shutdown failure-path model: PASS.
- Commit 16 callback ingress model: PASS.
- Focused Phase 8 CTest: 11/11 PASS.
- Clang 18 ThreadSanitizer: 18/18 PASS.
- Qt ownership regression: PASS.
- PJSIP regression: 45/45 PASS.
- Canonical configure/build and CTest: 146/146 PASS.
- Exact source manifest: 1 modified + 5 added files.
- Commit/push synchronization: PENDING at evidence capture.
- GitHub Actions for the new commit: PENDING.

The authorization regression executes the new production
header but does not exercise live PJSUA2 teardown.
The header is not wired into either Sip*CallClient::stop().

No real external callback-quiescence provider or PJ_EBUSY
recovery has been verified. Existing production shutdown
lifetime risks remain OPEN.
