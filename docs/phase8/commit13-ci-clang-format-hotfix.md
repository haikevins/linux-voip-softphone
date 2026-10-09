# Phase 8 Commit 13 — Full CI clang-format Hotfix

**Baseline:** `phase/8-multithreading` at `5d1e2c2` (Commit 12).
**Classification:** formatting-only for eleven tracked C++ files; adds a CI gate script, documentation, and observed evidence.
**Commit message:** `style(phase8): fix clang-format violations across Phase 8`.
**Git history:** append one commit after Commit 12; do not rewrite Commits 1–12.

## Why the initial four-file overlay was insufficient

The original GitHub Actions failure included `tests/qt/test_qt_registration_backend.cpp`.
The first hotfix corrected that file but a complete CI-equivalent run on the ThinkPad revealed
further formatting violations in ten Phase 8 C++ headers/tests. The ThinkPad formatted
**only those ten additional files** using Ubuntu `clang-format 18.1.3`.
This replacement overlay includes all eleven formatted C++ files, not just the Qt test.

## Exact source manifest: eleven MODIFIED files

- `src/concurrency/SerialTaskWorker.hpp`
- `src/concurrency/ThreadSafeQueue.hpp`
- `src/pjsip/CallbackActivityGate.hpp`
- `src/pjsip/PjsipCallSlotCleanup.hpp`
- `tests/phase8/test_mpmc_stress.cpp`
- `tests/phase8/test_pjsip_call_slot_cleanup.cpp`
- `tests/phase8/test_pjsip_callback_activity.cpp`
- `tests/phase8/test_serial_task_worker.cpp`
- `tests/phase8/test_thread_ownership.cpp`
- `tests/phase8/test_thread_safe_queue.cpp`
- `tests/qt/test_qt_registration_backend.cpp`

In addition, the overlay contains exactly three NEW files:

- `scripts/phase8/verify-ci-clang-format.sh`
- `docs/phase8/commit13-ci-clang-format-hotfix.md` (this file)
- `evidence/phase8/ci-clang-format-hotfix.txt`

**Total: 14 paths — 11 modified and 3 new.** No SIP client production `.cpp`, Qt GUI implementation,
CMake definitions, or pre-existing scripts are changed. Formatting edits to two PJSIP helper
headers cause their production consumers to rebuild, but do not claim a behavior change.

## Exact CI format gate

The `.github/workflows/static-analysis.yml` format check uses:

```sh
find src tests labs -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 \
  | xargs -0 clang-format --dry-run --Werror
```

Run `bash scripts/phase8/verify-ci-clang-format.sh` with the formatter version used
on the ThinkPad. The gate covers all C++ headers/sources under `src`, `tests`, and `labs`,
not just the files changed in this commit.

## Source and artifact provenance

The eleven C++ file payloads are byte-for-byte identical to the maintainer-provided
`phase8-commit13-formatted-source-review.zip`, SHA256
`0217e11caf0fff6abb9bfe2227e624ecbe90bd3b42daf7b1f8186fd1d852e5e9`.
Pre-format source SHA256 values were compared with the last contributing Phase 8
commit overlays (Commits 2, 3, 4, 5, 8, 10, and 11). All matched.

When applying the ZIP from a clean baseline, verify exact paths and baseline hashes.
Use `rsync --no-times` (or equivalent) to avoid copying future ZIP timestamps into
CMake/Ninja inputs. The maintainer already has these exact eleven formatted files
in their working tree; only the updated documentation/evidence needs replacement.

## Observed ThinkPad automated gates, 2026-10-09

- Ubuntu `clang-format 18.1.3`: full `src tests labs` CI-format gate **PASS**.
- Canonical CMake configure (PJSIP libpjproject 2.17) and production/Qt rebuild **PASS**.
- Qt ownership regression **PASS** (1/1).
- Focused Phase 8 CTest **PASS** (7/7).
- Clang 18 isolated TSan **PASS** (six C++ tests × three repetitions = 18 executions).
- Full canonical CTest **PASS** (142/142).
- `git diff --check` **PASS**.

The observed checks were performed against the 11 formatted source files before
this docs/evidence update. Adding the documentation/evidence does not alter their
compiled outputs. This document records observed logs, not invented local runs.

## Acceptance before Git commit

1. Verify current branch, HEAD `5d1e2c2`, and remote parity.
2. Verify every source file and the verification script match this finalized ZIP.
3. Confirm the full CI format PASS log, canonical build, Qt, Phase 8, TSan and
   canonical CTest PASS logs (or rerun if the source tree differs).
4. Record/stage exactly 14 paths, check `git diff --cached --check`, no unstaged
   or unexpected untracked files, and review the staged diff.
5. Commit and push to `phase/8-multithreading`; fetch, verify local and remote
   HEADs match and working tree is clean.
6. Inspect GitHub Actions on the **new** commit. Local formatting PASS does
   **not** prove remote CI PASS; older commit checks remain attached to their SHAs.

## Safety scope

Formatting verification, successful CTests, and isolated TSan executions do not
prove live PJSUA2 callback quiescence, safe teardown on `PJ_EBUSY`, or application-wide
freedom from data races/use-after-free. Those Phase 8 production risks remain open.
