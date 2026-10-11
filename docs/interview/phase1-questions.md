# Phase 1 Interview Review — Modern C++ Foundation

Use the repository implementation, documentation, tests, and evidence as the basis for the answers. Explain the engineering decision and the observed behavior; do not memorize isolated one-line definitions.

## Review scope

Base each answer on the implementation, tests, and retained evidence. Distinguish design intent, observed behavior, and unverified boundaries.

- RAII and deterministic resource cleanup
- exclusive versus shared ownership
- copy/move semantics
- STL container trade-offs
- type-safe state/event modeling
- dependency injection and testability

## Questions and answers

### Ownership and resource management

#### When should the project use `unique_ptr` instead of `shared_ptr`?

`std::unique_ptr` models exclusive resource ownership; use it when one owner controls the lifetime. `std::shared_ptr` makes ownership shared through reference counting and should be used only when several owners must prolong that lifetime. `std::weak_ptr` observes a shared object without extending its lifetime and helps break shared-ownership cycles.

Repository reference: `labs/cpp/smart-pointers/main.cpp`.

#### What problem does RAII solve?

RAII couples resource acquisition and release to object lifetime. Deterministic destructors release owned resources on ordinary returns and on exception unwinding. The file-descriptor wrapper in the lab disables copying so two wrappers cannot independently close the same descriptor.

Repository reference: `labs/cpp/raii/FileDescriptor.hpp`.

#### How do copy and move semantics differ for owning types?

Copying creates another logically independent value, while moving transfers state from an existing object. A moved-from object remains valid but its state follows the type's specific contract, or is otherwise valid but unspecified. A unique owning wrapper usually disables copying and implements moving to prevent double release.

Repository reference: `labs/cpp/move-semantics/main.cpp`.

### Containers and type-safe modeling

#### Why does this phase prefer `vector` over `list` for state history?

`std::vector` stores elements contiguously, which supports efficient iteration, cache locality, and random access. `std::list` supports stable iterators and constant-time insertion/erase at a known position but pays for allocation and pointer traversal. State history mainly appends and iterates, so `vector` matches this project's access pattern; its reallocation can invalidate iterators and references.

#### How should the project choose between `map` and `unordered_map`?

`std::map` keeps keys ordered and typically offers logarithmic lookup using a tree. `std::unordered_map` uses hashing, has average constant-time lookup, and does not promise key order. The choice depends on ordering, hash quality, memory use, and access patterns; neither is always faster.

#### Why `enum class`?

`enum class` scopes enumerator names and does not implicitly convert values to integers, preventing unrelated state values from mixing accidentally. The call states are declared in `src/core/CallState.hpp`.

#### Why `std::optional`?

`std::optional` represents the presence or absence of a value without an invented sentinel. `CallStateMachine::peer()` and `CallStateMachine::last_error()` return optional data because the corresponding information does not exist in every state.

#### Why `std::variant`?

`std::variant` holds one value from a closed set of compile-time alternatives. Events are declared in `src/core/Event.hpp`; `std::visit` dispatches their handling in the call state machine without an untyped event payload.

### Design and testability

#### What is dependency injection here?

`CallStateMachine` receives its `StateChangeSink` through constructor dependency injection rather than constructing a console or UI observer. Tests can provide a fake sink to assert notifications independently of presentation code; this reduces coupling and makes state transitions easy to verify.

## Phase boundary

Phase 1 answers should stay grounded in the C++ core and focused labs. Networking, SIP, RTP, PJSIP, and Qt are later-phase concerns.

## References

- `docs/phase1/README.md`
- `docs/architecture/call-state-machine.md`
- `labs/cpp/`
- `src/core/`
- `tests/unit/test_call_state_machine.cpp`
