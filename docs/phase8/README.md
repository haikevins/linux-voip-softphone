# Phase 8 — Multithreading

Status: **in progress — Commit 1 isolated queue foundation only**.

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
This slice is intentionally stand-alone: the Phase 8 test is **not yet**
registered with the repository's top-level CTest. Registration and integration
must be reviewed against the actual top-level CMake files in a later commit.

## Proposed follow-up sequence (not yet implemented)

1. Define close/stop behavior under blocked waits and worker teardown;
   add deterministic lifecycle tests and, if justified, `std::stop_token`.
2. Introduce worker/event dispatch only where an ownership boundary is
   already documented and tests can preserve Phase 7 behavior.
3. Integrate the focused tests into the canonical repository test gate.
4. Expand to reproducible 100,000-event MPMC stress and failure injection.
5. Add independent ThreadSanitizer configuration, verify on a supported
   Linux toolchain, and record limitations if it cannot run.
6. Run Phase 7 + Phase 8 regression gates; document final observed results
   and close milestone only after manual checks.

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

Before committing the overlay on the canonical repository, also use its
existing build and full regression gate, for example:

```bash
cmake --build build/qt-pjsip-gui-debug -j"$(nproc)"
ctest --test-dir build/qt-pjsip-gui-debug --output-on-failure
```

The existing build tree may need reconfiguration if it was moved between
machines. Do not treat copied `build/` paths as a verified baseline.

## Manual matrix

Commit 1 changes no SIP or GUI runtime behavior. Manual SIP call checks are
**not executed** for the isolated queue slice. Before any future integration
with registration/call workers, test registration, outgoing/incoming calls,
remote rejection after Ringing, local/remote hangup, retry without
re-registration, and RTP diagnostics refresh/clear behavior.

## Risks and limitations

- Queue is unbounded; producers can outrun consumers and grow memory.
- Blocking pop currently has no `stop_token`; `close()` is the wake-up path.
- Destruction while any thread calls `push`/`pop`/`close` is not safe; owner
  must first join all users.
- FIFO is the serialization order of successful pushes, not an ordering
  guarantee across independent producers.
- `std::optional<T>` construction requires move-constructible `T`; failures
  during a throwing `T` move do not remove the front queue entry.
- Integration and full-repository regression remain **unverified here**.

## Exit criteria (all pending)

Worker stop/close semantics verified; correct event ownership preserved;
100,000-event MPMC integrity/stress result observed; supported TSan gate
recorded; canonical full regression passes; necessary manual SIP scenarios
pass; architecture/interview docs, sanitized evidence, and Git/PR closure
completed.
