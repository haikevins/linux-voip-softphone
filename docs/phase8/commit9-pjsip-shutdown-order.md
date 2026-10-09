# Phase 8 — Commit 9: bounded PJSUA2 Call shutdown ordering

**Baseline:** `phase/8-multithreading` @ `1404826` (Commit 8).
**Scope:** change the ordering inside two `stop() noexcept` methods only; preserve
all existing timeouts, exceptions, locks, account and endpoint teardown behavior.

## Why

At baseline, `SipOutgoingCallClient::stop()` and `SipIncomingCallClient::stop()`
reset the derived `pj::Call` before `wait_for_call_cleanup(2s)`. PJSUA2 docs
require keeping a Call instance for the active call's lifetime, and recommend
waiting for calls to disconnect before deleting the account.

Sources (PJSIP 2.17):
- https://docs.pjsip.org/en/2.17/pjsua2/using/call.html
- https://docs.pjsip.org/en/2.17/api/generated/pjsip/group/group__PJSUA__LIB__ACC.html

## Actual change

For each client, `stop()` now executes, in this order:

1. `hangup()` (unchanged).
2. `wait_for_call_cleanup(2s)` **while the derived callback target is alive**.
3. `incoming_callback_mutex_`, `call_callback_mutex_`, `call_object_mutex_`
   acquired in existing order; `call_.reset()` while guarded (unchanged locks).
4. `shutdown_account_safely()` with `force=false` (unchanged).
5. `account_.reset()`, `libDestroy()`, `endpoint_.reset()` (unchanged).

This mirrors the previously implemented `release_disconnected_call()` pattern
in the outgoing client. Nothing is changed in Qt, RTP media or call state.

## Test gates and limitations

- `tests/phase8/test_pjsip_shutdown_order.py`: 6 stdlib Python structural
  regressions. Tests both actual source paths and mutations (early destruction,
  missing wait/lock and duplicate reset). They are NOT C++ runtime tests.
- `scripts/phase8/verify-pjsip-shutdown-order.sh`: first runs structural
  regressions, then compiles `voip_pjsip` and `pjsip_config_tests` using the
  existing canonical PJSIP-enabled CMake tree and runs CTest `-L '^pjsip$'`.
  For assistant-only inspection, set `PHASE8_PJSIP_STATIC_ONLY=1`; output
  clearly states that runtime work has NOT RUN. No secrets are read.
- Existing canonical CTest must remain **139/139**. Run Qt gate separately.
- This changes production shutdown ordering; run real SIP test-account
  scenarios (early hangup, pending INVITE, incoming ringing and active media)
  before claiming callback quiescence or application-wide race freedom.

### Residual high-priority risk (explicit)

`wait_for_call_cleanup(2s)` can time out. In that case, the function still
continues to reset `pj::Call`, shut down the account, and destroy PJSIP; this
matches the prior bounded fallback and remains unverified during in-flight
callbacks. A successful **static** test or canonical PJSIP config suite does
NOT prove the timeout path safe. An explicit safe fallback for persistent
call slots must be separately designed/tested with real PJSIP callbacks.

The CTest `-L pjsip` suite tests PJSIP-related contracts, but does not itself
exercise a live remote SIP call. Do not present it as such.

**Evidence policy:** record only observed ThinkPad output in
`evidence/phase8/pjsip-shutdown-order.txt`. Do not record live SIP outcomes
without independently performing them, and never store SIP passwords.
