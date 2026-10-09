# Phase 8 — Multithreading

Status: **in progress — Commits 1–5 concurrency foundation, canonical CTest integration, and MPMC stress coverage**.

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
  Expected canonical count is now **139** (previous verified 138 + 1).
  This count remains a target until observed on the ThinkPad.
- `scripts/phase8/verify-queue.sh` runs all four tests; the separate
  `scripts/phase8/verify-stress.sh` repeats the dedicated stress CTest ten
  times by default (`PHASE8_STRESS_REPEATS=1..100`).
- Isolated Linux toolchain verification is recorded in
  `evidence/phase8/stress-test.txt`: 4/4 CTest, 100/100 repetitions of
  the stress binary, and 25 complete-suite cycles passed. Those results
  are **not** presented as ThinkPad/canonical results.
- No Qt/PJSIP runtime code or behavior is changed by Commit 5.

## Proposed follow-up sequence (not yet implemented)

1. Audit actual Qt/PJSIP owner/callback boundaries before integration.
2. Add independent ThreadSanitizer configuration, verify on supported
   Linux toolchains, and record limitations if it cannot run.
3. Consider minimal worker/event handoff integration only after dedicated
   lifetime/GUI/PJSIP ordering tests establish safe ownership.
4. Run Phase 7 + Phase 8 regression gates and relevant manual SIP checks;
   document final results before declaring milestone closure.

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
```

The guarded `integrate-canonical-ctest.sh` script was executed during
Commit 4 and its three-line CMake registration was committed. **Do not run
it again** for Commit 5. Confirm that `tests/CMakeLists.txt` already includes
`add_subdirectory(phase8)`; the fourth test is then discovered automatically
by the updated `tests/phase8/CMakeLists.txt`.

Before committing the overlay on the canonical repository, also use its
existing build and full regression gate, for example:

```bash
cmake --build build/qt-pjsip-gui-debug -j"$(nproc)"
ctest --test-dir build/qt-pjsip-gui-debug --output-on-failure
```

The existing build tree may need reconfiguration if it was moved between
machines. Do not treat copied `build/` paths as a verified baseline.

## Manual matrix

Commits 1–5 change no SIP or GUI runtime behavior. Manual SIP call checks
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
  through Commit 3, 138/138 after Commit 4). Commit 5 canonical 139-entry
  verification is pending ThinkPad execution.
- `SerialTaskWorker` accepts `std::function<void()>`, not move-only callable
  objects. Task exceptions are counted, not forwarded to a UI error handler.
  Queue/task memory usage is unbounded. Calling shutdown from a callback is
  rejected; destroying the owner from a callback is a contract violation.

## Exit criteria (partial)

Standalone worker stop/close, worker thread-ownership, and 100,000-event
MPMC integrity have automated test evidence. The 100,000-event stress gate
and final canonical regression must still pass on the maintainer's ThinkPad.
Application-level Qt/PJSIP ownership, supported TSan evidence, relevant
manual SIP/GUI checks (if runtime integration changes behavior), final
architecture/interview docs, and Git/PR/release closure remain pending.
