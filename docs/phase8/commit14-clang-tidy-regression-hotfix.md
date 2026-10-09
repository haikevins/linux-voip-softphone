# Phase 8 — Commit 14: clang-tidy CI regression hotfix

**Expected immediate parent:** Commit 13 `style(phase8): fix clang-format violations across Phase 8` (parent of Commit 13: `5d1e2c2`).
**Status:** ThinkPad automated verification **PASS**; GitHub Actions verification **PENDING**.
**Scope:** exactly three C++ test source files plus one gate script, this document and evidence file.

## CI failure reproduced from maintainer log

On Ubuntu 24.04 GitHub Actions, `.github/workflows/static-analysis.yml` runs `cmake --preset clang-tidy` and `cmake --build --preset clang-tidy -j 2`. The project's `.clang-tidy` enables performance rules and `WarningsAsErrors: '*'`.

The initial CI excerpt reported six errors:

- Four `performance-unnecessary-value-param` diagnostics at tests/phase8/test_thread_safe_queue.cpp, in lambdas taking `std::stop_token`.
- Two `performance-inefficient-vector-operation` diagnostics at tests/phase8/test_serial_task_worker.cpp, for `writers.emplace_back()` loops missing preallocation.

## Minimal modifications

1. Change four *lambda parameters* from `std::stop_token stop` to `const std::stop_token &stop`. A `std::jthread` supplies a stop token when invoking its callable. Each lambda only borrows it during invocation, and `wait_pop(stop)` continues receiving its own by-value token as required by the existing queue API. Neither the queue API nor cancellation policy is changed.
2. Add `writers.reserve(producers);` immediately after each `writers` vector declaration and before the two `emplace_back` loops. Reserve changes vector capacity, not task scheduling, task counts, or queue behavior.
3. Handle the follow-up lint error in `test_pjsip_callback_activity.cpp` by assigning `std::nullopt` to the moved-from `std::optional<Permit>` instead of calling `reset()` on it. `optional::operator=(nullopt_t)` resets the contained moved-from permit, so the subsequent assertion still checks that it cannot decrement the callback counter twice. This is an explicit reinitialization recognized by `bugprone-use-after-move`.
4. Preallocate the four `std::jthread` slots using `threads.reserve(4);` before the `emplace_back` loop; the thread count and admission scenario remain unchanged.
5. Add `scripts/phase8/verify-ci-clang-tidy.sh` reproducing the exact CI configure/build presets with stdout check marker. This is **not** a bypass or suppression of clang-tidy checks.

No Qt code, PJSIP production code, CMake presets, `.clang-tidy` rules, or callback lifetime logic is changed.

## Subsequent real ThinkPad lint discovery — 2026-10-09

The initial five-file overlay passed the full clang-format gate on the ThinkPad, but the full `clang-tidy` build then reported **two more errors** in `tests/phase8/test_pjsip_callback_activity.cpp`:

- `bugprone-use-after-move` at moved-from `first.reset()` in the callback permit test.
- `performance-inefficient-vector-operation` at a `threads.emplace_back()` loop lacking `reserve(4)`.

The six errors from the earlier CI excerpt were not reported again before this later target failed; **this is not a complete clang-tidy PASS**. The replacement six-file overlay addresses all eight known diagnostics. The new source changes must be verified by the full clang-tidy gate on ThinkPad; other errors may still be discovered.

## Required ThinkPad acceptance before staging/commit

- Verify Commit 13 was committed and pushed; identify its real HEAD SHA. If not, finish Commit 13 first.
- Verify exact ZIP and baseline SHA256 for all three modified files; apply only the six overlay files.
- Run `bash scripts/phase8/verify-ci-clang-format.sh` (full `src/tests/labs` format gate).
- Run `bash scripts/phase8/verify-ci-clang-tidy.sh` (CI configure/build exact preset; **mandatory**).
- Run `bash scripts/phase8/verify-queue.sh` (focused 7/7), `verify-qt-ownership.sh`, canonical configure/build and canonical CTest (142/142).
- Run Clang 18 isolated TSan six tests x three (18/18).
- Record actual ThinkPad evidence, verify Git staging exact six paths, commit/push and confirm local/remote parity and clean tree.
- Confirm GitHub Actions run for the resulting commit. Local clang-tidy passing does not prove future CI jobs will be green.

**Note:** The user-supplied failure excerpt may omit diagnostics from other targets. This overlay addresses the eight reported errors; full CI clang-tidy build is the gate for any additional findings. Do not claim full lint PASS until the gate completes.

**Limitations:** This formatting/performance-lint fix is unrelated to live PJSUA2 callback quiescence, which remains unverified.


## THINKPAD VERIFIED COMMIT 14 V2

Recorded: 2026-10-10T00:09:29+07:00

Baseline: `phase/8-multithreading` at `2ce38f2`.

Verification performed on the maintainer ThinkPad:

- Full clang-format 18.1.3: PASS.
- Full clang-tidy 18.1.3 preset build: PASS.
- Focused Phase 8 CTest: 7/7 PASS.
- Clang 18 ThreadSanitizer: 18/18 executions PASS.
- Qt ownership regression: PASS.
- PJSIP CTest: 45/45 PASS.
- Canonical CMake build and CTest: 142/142 PASS.
- Exact three modified and three new files: PASS.

These checks verify the local automated regression gates.
GitHub Actions for the new commit remains PENDING.

No production SIP code was changed. Live PJSUA2 callback
quiescence and account shutdown failure recovery are not
established by this hotfix.
