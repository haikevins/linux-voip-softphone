# Phase 1 Interview Review — Modern C++ Foundation

Use the repository implementation, documentation, tests, and evidence as the basis for the answers. Explain the engineering decision and the observed behavior; do not memorize isolated one-line definitions.

## Review scope

- RAII and deterministic resource cleanup
- exclusive versus shared ownership
- copy/move semantics
- STL container trade-offs
- type-safe state/event modeling
- dependency injection and testability

## Questions and answers

### Ownership and resource management

#### `unique_ptr` vs `shared_ptr`

Explain ownership first. Use `unique_ptr` when one owner controls a resource. Use `shared_ptr`
when ownership genuinely needs to be shared. Explain why `weak_ptr` can observe an object without
extending its lifetime and how it can help avoid ownership cycles.

Repository reference: `labs/cpp/smart-pointers/main.cpp`.

#### What problem does RAII solve?

Explain how resource acquisition is coupled to object lifetime and how deterministic destructors
make cleanup happen on normal returns and exception paths. Point to the `FileDescriptor` wrapper
and explain why copying is disabled.

Repository reference: `labs/cpp/raii/FileDescriptor.hpp`.

#### Copy vs move

Explain that copying creates an independent logical value/resource while moving transfers state
from an object that remains valid but has an unspecified or explicitly defined moved-from state.
Explain why owning resource wrappers often delete copy operations and implement move operations.

Repository reference: `labs/cpp/move-semantics/main.cpp`.

### Containers and type-safe modeling

#### `vector` vs `list`

Discuss contiguous storage, cache locality, random access, iterator invalidation, insertion/erase
costs, and why `vector` is usually the default sequence container unless requirements indicate
otherwise. In this project, state history is stored in a vector because appending and sequential
iteration are the main operations.

#### `map` vs `unordered_map`

Discuss ordered tree semantics and logarithmic lookup for `map` versus hash-based average constant
time lookup and no ordering guarantee for `unordered_map`. Do not claim one is always faster.

#### Why `enum class`?

Explain scoped enumerator names, no implicit integer conversion, and stronger type safety.
Repository reference: `src/core/CallState.hpp`.

#### Why `std::optional`?

Explain how it models presence/absence explicitly instead of sentinel values. Repository reference:
`CallStateMachine::peer()` and `CallStateMachine::last_error()`.

#### Why `std::variant`?

Explain how it models one value from a closed set of alternatives with compile-time type safety.
Repository reference: `src/core/Event.hpp` and the `std::visit` call in the state machine.

### Design and testability

#### What is dependency injection here?

Explain that `CallStateMachine` receives the `StateChangeSink` dependency from outside through its
constructor. The core does not construct a console/UI implementation itself. Tests inject a fake
sink, which improves testability and reduces coupling.

## Phase boundary

Phase 1 answers should stay grounded in the C++ core and focused labs. Networking, SIP, RTP, PJSIP, and Qt are later-phase concerns.

## References

- `docs/phase1/README.md`
- `docs/architecture/call-state-machine.md`
- `labs/cpp/`
- `src/core/`
- `tests/unit/test_call_state_machine.cpp`
