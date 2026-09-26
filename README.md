# Linux VoIP Softphone

A Linux desktop SIP softphone engineering portfolio developed progressively with C++20,
Qt/QML, PJSIP, Linux system programming, networking, testing, and debugging tooling.

## Current milestone

**Phase 1 — Modern C++ Foundation**

Phase 0 established the reproducible engineering environment. Phase 1 now adds the first
production-oriented C++ core and focused learning labs for ownership and value semantics.

## Phase 1 learning scope

```text
RAII
smart pointers
move semantics
const correctness
enum class
std::optional
std::variant
STL containers
interfaces
basic dependency injection
```

## Start here

Read the package/application note first:

```text
PHASE1_START_HERE.md
```

Then follow the full Phase 1 tutorial:

```text
docs/phase1/README.md
```

The complete project roadmap is preserved in:

```text
README_roadmap.md
```

## Phase 1 source layout

```text
labs/cpp/
├── raii/
├── smart-pointers/
└── move-semantics/

src/core/
├── CallState.cpp
├── CallState.hpp
├── CallStateMachine.cpp
├── CallStateMachine.hpp
├── Event.hpp
├── StateChangeSink.cpp
├── StateChangeSink.hpp
└── Types.hpp

tests/unit/
├── test_build_info.cpp
├── test_call_state.cpp
└── test_call_state_machine.cpp
```

## Build and test

```bash
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug --output-on-failure
```

Run the production-core demo:

```bash
./build/debug/src/linux-voip-softphone
```

Run the C++ labs:

```bash
./build/debug/labs/cpp/raii/lab_raii
./build/debug/labs/cpp/smart-pointers/lab_smart_pointers
./build/debug/labs/cpp/move-semantics/lab_move_semantics
```

## Quality gates

```bash
./scripts/verify_phase1.sh
```

The verification covers:

```text
clang-format
debug build
GoogleTest
RAII/smart-pointer/move labs
ASan + UBSan
clang-tidy
```

Collect evidence generated on your own Ubuntu machine:

```bash
./scripts/collect_phase1_evidence.sh
```

Evidence is written to:

```text
evidence/01-cpp/
```

## Architecture artifact

The Phase 1 roadmap requires:

```text
docs/architecture/call-state-machine.md
tests/unit/test_call_state.cpp
```

The architecture document explains the state model, `enum class`, `std::variant`, `std::optional`,
STL container choice, interface abstraction, and constructor dependency injection.

## Milestone

After local verification and all GitHub Actions are green, create:

```bash
git tag -a v0.2-cpp-core \
    -m "Phase 1: Modern C++ foundation and call state core"

git push origin v0.2-cpp-core
```

Do not create the tag before the Phase 1 Definition of Done is satisfied.
