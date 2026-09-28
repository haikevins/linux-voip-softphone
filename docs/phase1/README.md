# Phase 1 — Modern C++ Foundation

## Status

**Completed** — milestone tag `v0.2-cpp-core`.

Phase 1 establishes the Modern C++ patterns used by later Linux, networking, SIP, media, and UI code.

## Objective

Move from a build-system skeleton to production-oriented C++20 code with explicit resource ownership, strong state types, testable interfaces, and focused learning labs.

## Scope

Included in this phase:

- RAII;
- `std::unique_ptr`, `std::shared_ptr`, and `std::weak_ptr` concepts;
- move semantics and non-copyable resource owners;
- const correctness;
- `enum class`;
- `std::optional`;
- `std::variant` and `std::visit`;
- STL container choices;
- interface abstraction;
- constructor dependency injection;
- call-state transition tests.

Deliberately excluded:

- POSIX process/signal/IPC work;
- TCP/UDP networking;
- SIP/PJSIP and media handling.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| RAII lab | Demonstrate deterministic file-descriptor ownership | `labs/cpp/raii/` |
| Smart-pointer lab | Demonstrate ownership relationships | `labs/cpp/smart-pointers/` |
| Move-semantics lab | Demonstrate transfer of ownership/value state | `labs/cpp/move-semantics/` |
| Call-state model | Define strongly typed call states and transitions | `src/core/CallState.*`, `src/core/CallStateMachine.*` |
| Events | Represent state-machine inputs with `std::variant` | `src/core/Event.hpp` |
| State-change abstraction | Decouple state transitions from observers | `src/core/StateChangeSink.*` |
| Unit tests | Verify state semantics and invalid transitions | `tests/unit/test_call_state*.cpp` |
| Architecture note | Explain the design decisions | `docs/architecture/call-state-machine.md` |
| Interview review | Capture Phase 1 review questions | `docs/interview/phase1-questions.md` |

## Architecture and design

The Phase 1 core separates state representation, event representation, transition logic, and observation:

```text
Event (`std::variant`)
        |
        v
CallStateMachine
        |
        +--> CallState (`enum class`)
        +--> optional peer/error data
        +--> transition history
        |
        v
StateChangeSink interface
```

Key design choices:

- resource owners are non-copyable when duplicate ownership would be unsafe;
- move operations transfer ownership and leave the source non-owning;
- `enum class` prevents accidental integer-style state mixing;
- `std::optional` represents values that may legitimately be absent;
- `std::variant` makes the event set explicit and type-safe;
- dependencies are provided through interfaces rather than hard-coded globals;
- invalid transitions are rejected and tested.

## Build and run

Build and run the normal debug test suite:

```bash
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug --output-on-failure
```

The focused learning programs are built with the same project and can be run from the debug build tree:

```text
lab_raii
lab_smart_pointers
lab_move_semantics
```

## Verification

The phase verification includes:

- unit tests for the call-state model;
- focused lab execution;
- `clang-format`;
- `clang-tidy`;
- ASan and UBSan;
- `git diff --check`;
- CI verification.

The repository helper for the phase is:

```bash
./scripts/verify_phase1.sh
```

## Evidence

Committed evidence lives under:

```text
evidence/01-cpp/
```

It includes representative build/test/static-analysis/sanitizer output and lab execution results generated from the development environment.

## Interview focus

Use `docs/interview/phase1-questions.md` together with the implementation. Be ready to explain:

- RAII and deterministic cleanup;
- `unique_ptr` versus `shared_ptr`;
- the role of `weak_ptr`;
- copy versus move semantics;
- why `std::move` enables a move rather than performing one by itself;
- `vector` versus `list`;
- `map` versus `unordered_map`;
- `enum class`, `optional`, and `variant`;
- interface abstraction and constructor dependency injection.

## Exit criteria

Phase 1 is complete when all of the following statements are true:

- the C++ labs build and run;
- the call-state core is covered by unit tests;
- invalid transitions are testable and rejected;
- resource ownership is explicit and move-aware;
- formatting, static analysis, sanitizers, tests, and CI pass;
- the architecture note and interview review are present;
- Phase 1 evidence is committed;
- milestone tag `v0.2-cpp-core` identifies the completed phase.

## References

- Architecture: `docs/architecture/call-state-machine.md`
- Interview review: `docs/interview/phase1-questions.md`
- Evidence: `evidence/01-cpp/`

## Next phase

Phase 2 applies RAII and ownership rules to real Linux file descriptors, processes, signals, and Unix Domain Socket IPC.
