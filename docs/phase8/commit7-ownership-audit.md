# Phase 8 — Commit 7: Qt/PJSIP ownership audit entry gate

**Status: source-level review PENDING.** This commit adds a reproducible,
read-only hotspot inventory of the canonical application sources. It is NOT a
claim that Qt/PJSIP callback safety, lifetime, or shutdown ordering has been
established. The assistant did not have the canonical application source
snapshot while preparing this ZIP; do not infer runtime design from the Phase 7
handoff alone.

## Purpose and scope

- Audit actual source on `phase/8-multithreading` at parent `ae870ff`.
- Find candidate PJSUA2 callbacks, Qt queued/direct handoff and thread checks,
  owned workers, teardown methods, locks, and application call-state boundaries.
- Output **relative paths, line numbers and static categories only**. No lines of
  source text, matched literals, SIP credentials or absolute local paths.
- Preserve all existing Qt/PJSIP/VoIP functionality; the commit adds *no*
  production integration or runtime thread changes.
- The scanner uses lexical categories, so it can miss indirect call paths,
  overloaded symbols, macro-generated code, and library callbacks. Matches are
  **not findings**. Absence of matches is **not a safety proof**.

## Verification in canonical clone

Run after overlay and SHA256 validation, while Git HEAD is still `ae870ff`:

```bash
cd ~/Documents/linux-voip-softphone
set -euo pipefail
bash -n scripts/phase8/verify-ownership-audit.sh
bash scripts/phase8/verify-ownership-audit.sh \
    2>&1 | tee /tmp/phase8-commit7-ownership-inventory.txt
```

The wrapper first executes five Python fixture tests, checks the branch and
baseline, and refuses uncommitted production-source modifications. It prints an
`INCONCLUSIVE` review checklist after a successful *inventory*. This must not
be interpreted as runtime safety PASS. Run the existing canonical CMake build
and all 139 CTest entries before committing.

## Human review questions (still required)

1. Determine which thread each PJSUA2 `Account`/`Call` callback executes on
   and whether PJSIP uses external or internally registered callback threads.
2. Identify the Qt QObject/QML owner thread and the `CallManager` owner-thread
   contract. Verify that a queued handoff happens **before** touching owner
   state, and that the target lifetime outlasts the queued delivery.
3. Trace `stop_token`, `std::jthread`, callback guard and endpoint shutdown:
   prevent callbacks from using destroyed accounts, endpoints or QObjects.
4. Check registration and call-state callbacks in flight while closing window,
   disconnecting account, canceling registration, and tearing down session.
5. Verify no `SerialTaskWorker` callback invokes PJSUA2 or Qt UI APIs without
   an independently proved handoff and ownership contract.
6. Validate the resulting assertions through actual tests and, where feasible,
   manually reproduce shutdown races with a SIP test server. Avoid real
   credentials in logs and evidence.

## Acceptance and non-goals

The audit inventory can be committed when its fixture tests pass, canonical
regression remains 139/139, and `evidence/phase8/ownership-audit.txt` truthfully
records **INCONCLUSIVE / actual review pending**. A follow-up implementation
commit must audit the identified source locations and add focused regression
before declaring Phase 8 runtime ownership complete.

No changes to `tests/CMakeLists.txt` or the 139-entry CTest registration are
necessary for this source-only tooling increment.
