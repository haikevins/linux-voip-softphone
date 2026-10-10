# Phase 8 — Commit 19: PJSUA2 shutdown authorization integration contract

**Baseline (required before overlay):** `phase/8-multithreading` at
`1e88ea455f157622fbcd8c18b0df60b4287ed5af` (Commit 18).

**Proposed commit:** `test(phase8): gate unsafe PJSIP shutdown authorization integration`.

## Why this commit does not wire the authorization helper into stop()

Commit 18 introduced `src/pjsip/PjsipShutdownAuthorization.hpp` as real
production-source *bookkeeping*, not a live safety proof. An incoming/outgoing
`stop() noexcept` callback lifetime fix is **not safe to infer** from the four
receipt names. An independent source of `ExternalCallbackIngressQuiescent`
proof has **not** been established by libpjproject 2.17 / PJSUA2, and the
shutdown path still lacks a destructor-safe ownership escape/retry mechanism.

Commit 19 adds a **source integration blocker gate**, deliberately leaving
both production `stop()` implementations unchanged. The gate inspects the
*actual* incoming and outgoing C++ files and the exact authorization header;
no SIP network, PJSUA2 instance, credentials, or live callback is exercised.
A PASS means **the known unsafe integration blockers were recognized**, not
that teardown became safe.

## Real production findings (both SIP clients)

The checked `SipIncomingCallClient::stop()` and
`SipOutgoingCallClient::stop()` currently:

1. Discard the `wait_for_call_cleanup(std::chrono::seconds{2})` result.
2. Reset the derived `pj::Call` callback object even after possible timeout.
3. Discard `shutdown_account_safely()` success/failure, including `PJ_EBUSY`.
4. Reset `pj::Account` despite possible account-shutdown failure.
5. Call `libDestroy()` and reset `pj::Endpoint` without independent callback
   quiescence assurance.
6. Have callbacks borrowing the client owner via `owner_` references; the
   registration callback has no corresponding teardown activity gate.

The pre-existing Phase 8 source auditor enumerates **six named risks per
client**. The new integration contract relies on that auditor, verifies that
all four distinct `ShutdownProof` labels are present, and rejects new helper
wiring or `ShutdownProof::...` receipts in either client until source-review.
**It does not prove that no other dynamic path can create unsafe callbacks.**

## Static regression and fail-closed behavior

`tests/phase8/test_pjsip_shutdown_integration_contract.py` includes 12
regressions: inspect both real production sources, four proof labels, confirm
current BLOCKED result and six risks per client; mutation cases reject
counterfeit external quiescence from local drain, premature helper include,
reordered call destruction, and source changes that remove known audit
patterns without a deliberate re-review. Comment-only token mutations are
ignored so a documentation comment is not mistaken for runtime wiring.

`--require-ready` is a **negative safety guard**: it exits status `3` when
known blockers are present; the verification script demands that exact
failure. Unexpected changed sources return status `2` and require manual
reassessment. The default audit command exits zero only when it has reliably
identified the **current blocked** baseline; it must not be used as a
readiness signal. Any legitimate production repair must update this gate,
risks, and independent live tests as part of an intentional reviewed change.

The new Python CTest runs only under `NOT PHASE8_ENABLE_TSAN`. The existing
standalone Clang18 TSan suite remains **six C++ tests × three runs (18/18)**.
Expected focused suite: **12/12**; expected canonical suite: **147/147**;
existing PJSIP regression: **45/45**.

## Exact overlay and verification process

- M `tests/phase8/CMakeLists.txt`
- A `scripts/phase8/audit-pjsip-shutdown-integration.py`
- A `tests/phase8/test_pjsip_shutdown_integration_contract.py`
- A `scripts/phase8/verify-pjsip-shutdown-integration-contract.sh` (`100755`)
- A `docs/phase8/commit19-pjsip-shutdown-integration-contract.md`
- A `evidence/phase8/pjsip-shutdown-integration-contract.txt`

Before applying the ZIP, check exact baseline SHA, branch, origin parity,
clean tree, CMake source SHA, ZIP SHA256 and six-file manifest. Apply with
`rsync --no-times --checksum`; verify staged source bytes against ZIP.
Run full ThinkPad clang-format/clang-tidy first, then new focused+canonical
audit script, previous Commit 18/17/16/15 gates, focused 12/12, TSan 18/18,
Qt, PJSIP 45/45, and canonical 147/147. Only observed local results may
replace PENDING evidence. Then stage exactly six files, commit, push and
verify origin parity, clean tree, and GitHub Actions separately. **Do not
merge the feature branch into main.**

## Future integration prerequisites (not supplied by this commit)

Document a PJSUA2-supported external callback quiescence contract for the
real library/runtime; ensure each `Call`, `Account`, and `Endpoint` object
outlives every callback that may reference it; specify ownership behavior
for `stop() noexcept` and destructors when quiescence is unprovable; handle
`PJ_EBUSY`, cleanup timeout, and late callbacks with real runtime tests.
Never derive external proof from the local gate alone. Any production wiring
before those prerequisites is **BLOCKED** by policy, regardless of passing CI.


## THINKPAD VERIFIED PHASE 8 COMMIT 19

Recorded: 2026-10-10T08:19:01+07:00

Branch: `phase/8-multithreading`
Baseline HEAD: `1e88ea455f157622fbcd8c18b0df60b4287ed5af`

Maintainer ThinkPad verification:

- Full clang-format 18.1.3: PASS.
- Full clang-tidy 18.1.3: PASS.
- Commit 19 blocker audit: PASS; readiness remains BLOCKED.
- Commit 18 authorization header: PASS.
- Commit 17 ownership policy model: PASS.
- Commit 16 callback ingress model: PASS.
- Commit 15 failure-path model: PASS.
- Focused Phase 8 CTest: 12/12 PASS.
- Clang18 ThreadSanitizer: 18/18 PASS.
- Qt ownership regression: PASS.
- PJSIP regression: 45/45 PASS.
- Canonical configure/build and CTest: 147/147 PASS.
- Exact overlay: 1 modified + 5 added files.
- Commit/push: PENDING at evidence capture.
- GitHub Actions on new commit: PENDING.

Important: Integration readiness is intentionally BLOCKED.
The gate confirms that existing unsafe shutdown conditions
remain identifiable. It is not a live PJSUA2 safety test.

External callback quiescence: UNPROVEN.
Actual PJ_EBUSY recovery: UNPROVEN.
Production lifetime risks: OPEN.
