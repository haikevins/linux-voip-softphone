# Phase 8 — Multithreading

Status: **in progress — Commits 1–4 concurrency foundation and test integration**.

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

## Proposed follow-up sequence (not yet implemented)

1. Audit actual Qt/PJSIP owner/callback boundaries before any integration.
2. Add reproducible 100,000-event MPMC stress and failure injection.
3. Add independent ThreadSanitizer configuration, verify on supported
   Linux toolchains, and record limitations if it cannot run.
4. Consider minimal worker/event handoff integration only after dedicated
   lifetime/GUI/PJSIP ordering tests establish safe ownership.
5. Run Phase 7 + Phase 8 regression gates and relevant manual SIP checks;
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
```

After verifying the ZIP hash and overlay paths, run the guarded
registration script once and inspect its resulting Git diff:

```bash
bash scripts/phase8/integrate-canonical-ctest.sh
git diff -- tests/CMakeLists.txt
```

Before committing the overlay on the canonical repository, also use its
existing build and full regression gate, for example:

```bash
cmake --build build/qt-pjsip-gui-debug -j"$(nproc)"
ctest --test-dir build/qt-pjsip-gui-debug --output-on-failure
```

The existing build tree may need reconfiguration if it was moved between
machines. Do not treat copied `build/` paths as a verified baseline.

## Manual matrix

Commits 1–4 change no SIP or GUI runtime behavior. Manual SIP call checks
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
- Phase 8 queue/worker runtime integration remains unverified; the
  canonical Phase 7 regression passed after Commits 1, 2 and 3 (maintainer
  logs). Commit 4's top-level registration remains pending canonical verification.
- `SerialTaskWorker` accepts `std::function<void()>`, not move-only callable
  objects. Task exceptions are counted, not forwarded to a UI error handler.
  Queue/task memory usage is unbounded. Calling shutdown from a callback is
  rejected; destroying the owner from a callback is a contract violation.

## Exit criteria (all pending)

Worker stop/close semantics verified; correct event ownership preserved;
100,000-event MPMC integrity/stress result observed; supported TSan gate
recorded; canonical full regression passes; necessary manual SIP scenarios
pass; architecture/interview docs, sanitized evidence, and Git/PR closure
completed.
