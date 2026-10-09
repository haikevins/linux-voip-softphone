# Phase 8 — Multithreading

Status: **in progress — Commits 1–5 verified on ThinkPad; Commit 6 standalone ThreadSanitizer verified with Clang 18 on ThinkPad**.

Baseline: Phase 7 release `v0.8-qt-gui` (`main` at `2bd8740` in the
handoff). Do not change the release tag or reopen Phase 7 behavior without
new regression evidence.

## Goal

Make the softphone's concurrency model explicit and testable: safe event
handoff, deterministic worker lifecycle, bounded ownership, and race/stress
verification. The release already has `std::jthread`, mutex/condition-variable
and Qt queued handoff; Phase 8 is not an instruction to move all PJSIP or Qt
work to new threads.

## Commit 1: implemented here

- `src/concurrency/ThreadSafeQueue.hpp`: a standalone C++20 unbounded MPMC
  FIFO with `push`, `try_pop`, `wait_pop`, and `close`.
- `tests/phase8/`: independent CMake/CTest target with six behavioral
  scenarios, including a 4-producer/4-consumer 4,000-item integrity check.
- `scripts/phase8/verify-queue.sh`: repeatable focused verification.
- `docs/architecture/threading-model.md`: separation of the current Phase 7
  handoff and proposed Phase 8 extensions.

**No existing PJSIP, Qt, GUI, call-state, or call-media source is modified.**
Commit 1 was intentionally standalone. Commit 4 adds a guarded, reviewable
path to register the focused tests with the canonical top-level CTest.

## Commit 2: implemented here

- `wait_pop(std::stop_token)` uses C++20 `std::condition_variable_any` for
  cancellation-aware waiting. Existing `wait_pop()` and `close()` behavior
  is unchanged.
- Cancellation is **local to the waiting consumer**: it does not close the
  shared queue, reject pushes, or discard queued items. A stop observed
  before dequeue wins; concurrent stop/dequeue may resolve either way.
- Five additional standalone test scenarios cover pre-requested stop,
  blocked waiter wake-up, queue reuse after one worker stops, draining
  after close, and close/join of multiple stoppable workers. Total: **11
  executable scenarios**.
- Commit 2's canonical ThinkPad test evidence (1/1 focused, 135/135
  existing regression) and pushed commit `a936437` are recorded from the
  maintainer's 2026-10-09 terminal log.

## Commit 3: implemented here

- `src/concurrency/SerialTaskWorker.hpp`: one owned `std::jthread` and FIFO
  task handoff using the existing `ThreadSafeQueue<std::function<void()>>`.
- `post()` accepts callable tasks until `request_close()` / `shutdown()`;
  accepted tasks execute once in serial FIFO order. Empty callables are
  rejected. `shutdown()` closes the queue, drains work, and joins; the
  destructor performs the same orderly teardown.
- `request_close()` may be called by the worker callback, but `shutdown()`
  refuses a self-join. Concurrent **external** shutdown callers serialize
  their join. The owner must synchronize destruction with concurrent API
  users; destruction from within a callback is unsupported.
- Throwing callbacks do not terminate the worker: `failures()` counts their
  exceptions and `completed()` counts every finished callback, including
  failed ones. Exception details are not retained.
- Dedicated independent worker CTest adds 10 behavioral scenarios. Focused
  verification now registers two CTest entries (queue + worker), with 21
  executable scenarios. No Qt or PJSUA2 runtime integration is claimed.
- `scripts/phase8/verify-queue.sh` retains its path and runs both targets.

## Commit 4: canonical CTest registration and worker ownership tests

- `tests/phase8/CMakeLists.txt` supports both standalone configuration and
  inclusion via `add_subdirectory(phase8)` under the canonical `tests/` tree;
  it does not create a nested CMake project when included by the app.
- New `Phase8ThreadOwnershipTest` verifies that callbacks from four
  producer threads run only on one stable worker thread and that nested
  `post()` operations are queued, not executed reentrantly. This verifies
  **SerialTaskWorker ownership only**, not Qt/PJSIP `CallManager` ownership.
- `scripts/phase8/integrate-canonical-ctest.sh` makes one guarded append to
  the existing `tests/CMakeLists.txt` **on the canonical ThinkPad clone**.
  That file is not shipped as an unverifiable overwrite in the ZIP; the
  script requires branch `phase/8-multithreading`, baseline HEAD `9358f8b`,
  a tracked/unchanged tests/CMakeLists.txt, and the new test files.
- Once the script has run, inspect `git diff -- tests/CMakeLists.txt` and
  reconfigure/rebuild the canonical project before running `ctest`. Based
  on the prior 135-entry suite, **138** entries are expected, but the exact
  count is not claimed until it is observed in the canonical clone.
- Commit 3 canonical results were supplied by the maintainer: focused
  **2/2** and existing regression **135/135**, commit `9358f8b` pushed.
- The maintainer subsequently provided the Commit 4 ThinkPad log: focused
  **3/3**, canonical **138/138**, `c7a4243` pushed, remote HEAD matching
  and clean working tree on 2026-10-09. See `evidence/phase8/automated-verification.txt`.

## Commit 5: 100,000-event MPMC stress and failure injection

- `Phase8MpmcStressTest` adds four independent stress scenarios:
  1. **100,000 distinct events**, four producers and four consumers, verified
     against atomic per-ID occurrence counts (no missing, duplicate, malformed,
     or prematurely rejected events); close and drain after producers join.
  2. 20,000 events across four producers and one consumer to verify FIFO for
     each producer. Delivery order is **not** inferred from the scheduling of
     separate consumer callbacks in the 4x4 test.
  3. Queue closure with producers still live and four consumers, accounting
     for each accepted/rejected push and draining all accepted IDs exactly
     once; close races may produce different acceptance counts per run.
  4. 10,000 tasks from four producers through `SerialTaskWorker`, including
     deterministic injected exceptions; every task is counted as completed
     and all exceptions are contained.
- `tests/phase8/CMakeLists.txt` registers a fourth CTest entry when included
  from the canonical project **or** configured as a standalone suite.
  Canonical ThinkPad verification confirmed **139/139** CTest entries.
- `scripts/phase8/verify-queue.sh` runs all four tests; the separate
  `scripts/phase8/verify-stress.sh` repeats the dedicated stress CTest ten
  times by default (`PHASE8_STRESS_REPEATS=1..100`).
- Isolated Linux toolchain verification is recorded in
  `evidence/phase8/stress-test.txt`: 4/4 CTest, 100/100 repetitions of
  the stress binary, and 25 complete-suite cycles passed. Those results
  are **not** presented as ThinkPad/canonical results.
- Maintainer terminal logs on 2026-10-09 confirm **4/4** focused tests,
  **100/100** repeated stress runs, canonical **139/139**, and clean/pushed
  `5979869` on `phase/8-multithreading`. The canonical evidence was appended
  to the tracked `automated-verification.txt` and `stress-test.txt` on the
  ThinkPad; do not overwrite those post-ZIP additions with old snapshots.
- No Qt/PJSIP runtime code or behavior is changed by Commit 5.

## Commit 6: independent ThreadSanitizer instrumentation

- `tests/phase8/CMakeLists.txt` accepts **opt-in**
  `-DPHASE8_ENABLE_TSAN=ON` in a **standalone** `tests/phase8` build only.
  Each of the existing four Phase 8 test executables is compiled and linked
  with `-fsanitize=thread` (GCC/Clang) plus debug/frame-pointer flags.
  The normal canonical project keeps the default OFF, so its 139 CTest
  entries and Qt/PJSIP binaries are not partially instrumented.
- `scripts/phase8/verify-tsan.sh` builds in the separate
  `build/phase8-tsan-debug` directory and fails if any TSan test fails. It
  refuses known non-TSan build directories, validates repeat count, and
  preserves the output under the disposable TSan build tree.
- TSan must **not** be combined with ASan or UBSan in the same executable.
  A sanitizer report fails the test gate; unsupported/runtime-incompatible
  environments must be recorded as BLOCKED, **not** as a clean run.
- On the isolated Linux environment, GCC 14.2 + `libtsan.so.2` passed four
  instrumented CTest entries three times each without a reported race.
  On the ThinkPad, GCC 13.3 TSan was BLOCKED by unexpected memory
  mappings, including in a minimal reproduction (3/3 failures).
  Clang 18.1.3 successfully ran the minimal probe 3/3 and all four
  instrumented Phase 8 tests three times each. See
  `evidence/phase8/tsan-report.txt`.
- No SIP registration, Qt/QML, call lifecycle, PJSIP, or RTP code changes.

## Proposed follow-up sequence (not yet implemented)

1. Preserve the ThinkPad Clang 18 TSan verification and rerun it
   after future changes to concurrency code.
2. Audit actual Qt/PJSIP owner/callback boundaries before integration.
3. Consider minimal worker/event handoff integration only after dedicated
   lifetime/GUI/PJSIP ordering tests establish safe ownership.
4. Run full regressions and relevant manual SIP checks; document final
   results before declaring milestone closure.

## In scope

- Thread ownership, queue invariants, clean shutdown, and event ordering.
- Race and lifetime tests and sanitizer evidence.
- Preserve SIP/call/registration/retry/RTP diagnostics behavior.

## Out of scope

- New GUI/call features, additional SIP accounts, dialplan changes,
  unrelated PJSUA2 refactors, and changes to the `v0.8-qt-gui` tag.
- Treating this queue as a replacement for Qt queued signals/slots or
  claiming PJSIP endpoint objects are thread-safe.

## Verification

Focused (no SIP, Qt, PJSIP, or GoogleTest dependency):

```bash
bash scripts/phase8/verify-queue.sh
bash scripts/phase8/verify-stress.sh
CXX="$(command -v clang++-18)" \
PHASE8_TSAN_BUILD_DIR="$PWD/build/phase8-tsan-clang18-debug" \
PHASE8_TSAN_REPEATS=3 \
    bash scripts/phase8/verify-tsan.sh
```

The guarded `integrate-canonical-ctest.sh` script was executed during
Commit 4 and its three-line CMake registration was committed. **Do not run
it again** for Commit 6. Confirm that `tests/CMakeLists.txt` includes
`add_subdirectory(phase8)`. This commit adds no new canonical test entry,
so the total should stay at **139**.

Before committing the overlay on the canonical repository, also use its
existing build and full regression gate, for example:

```bash
cmake --build build/qt-pjsip-gui-debug -j"$(nproc)"
ctest --test-dir build/qt-pjsip-gui-debug --output-on-failure
```

The existing build tree may need reconfiguration if it was moved between
machines. Do not treat copied `build/` paths as a verified baseline.

## Manual matrix

Commits 1–6 change no SIP or GUI runtime behavior. Manual SIP call checks
are **not executed** for these isolated concurrency slices. Before any future integration
with registration/call workers, test registration, outgoing/incoming calls,
remote rejection after Ringing, local/remote hangup, retry without
re-registration, and RTP diagnostics refresh/clear behavior.

## Risks and limitations

- Queue is unbounded; producers can outrun consumers and grow memory.
- `wait_pop(stop_token)` supports per-wait cancellation; the legacy
  `wait_pop()` still needs push/close to wake. `SerialTaskWorker` chooses
  graceful queue close-and-drain rather than stop-token cancellation.
- Destruction while any thread calls `push`/`pop`/`close` is not safe; owner
  must first join all users.
- FIFO is the serialization order of successful pushes, not an ordering
  guarantee across independent producers.
- `std::optional<T>` construction requires move-constructible `T`; failures
  during a throwing `T` move do not remove the front queue entry.
- Phase 8 queue/worker runtime integration remains unverified. Maintainer
  logs show the canonical regression passed after Commits 1–4 (135/135
  through Commit 3, 138/138 after Commit 4, **139/139** after Commit 5).
  ThinkPad GCC 13 TSan was BLOCKED by runtime mapping failures;
  ThinkPad Clang 18 TSan passed four tests three times each.
  Neither result establishes Qt/PJSIP runtime thread safety.
- `SerialTaskWorker` accepts `std::function<void()>`, not move-only callable
  objects. Task exceptions are counted, not forwarded to a UI error handler.
  Queue/task memory usage is unbounded. Calling shutdown from a callback is
  rejected; destroying the owner from a callback is a contract violation.

## Exit criteria (partial)

Worker stop/close, thread-ownership, and 100,000-event MPMC integrity have
ThinkPad evidence, including 100/100 stress repetitions and 139/139 canonical
CTest entries (Commit 5). The ThinkPad Clang 18 TSan suite passed
four instrumented tests three times each. Qt/PJSIP runtime thread
ownership verification remains pending.
Application-level Qt/PJSIP ownership, manual SIP/GUI checks if runtime code
changes, final architecture/interview docs, and Git/PR/release closure remain
pending.

## Commit 8 — Qt/PJSIP source ownership review (ThinkPad regression verified; live SIP pending)

See `docs/phase8/commit8-source-ownership-review.md` for the source-level
path/line audit based on `cf31e2a`. `scripts/phase8/verify-qt-ownership.sh`
executes the existing Qt registration CTest entry with two additional
fake-session ownership/lifetime regressions. This slice does not alter Qt/SIP
production code, PJSUA2 shutdown or the 139-entry CTest registration.
The production PJSUA2 callback lifetime review remains open.


## Commit 9 — PJSUA2 call shutdown-order hardening (ThinkPad build/CTest verified; live SIP pending)

See `docs/phase8/commit9-pjsip-shutdown-order.md`. Both outgoing and incoming
`stop()` paths now wait for bounded call-slot cleanup **before** destroying the
derived Call callback target. The timeout fallback is not proven safe with
in-flight PJSIP callbacks. `scripts/phase8/verify-pjsip-shutdown-order.sh`
provides static contract tests plus a required canonical PJSIP build/CTest
gate; test-account SIP runtime validation remains separate and pending.
