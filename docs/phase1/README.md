# Phase 1 — Modern C++ Foundation

Estimated time: **4–6 days**.

This phase follows the Phase 1 section of `README_roadmap.md`. The goal is not to add SIP or Qt
features yet. The goal is to create a C++ core that demonstrates ownership, lifetime, state
modeling, value semantics, testability, and clean separation of dependencies.

## Definition of Done

Phase 1 is complete only when all of the following are true on your Ubuntu machine:

```text
RAII lab                         PASS
smart pointer lab                PASS
move semantics lab               PASS
CallState unit tests             PASS
CallStateMachine unit tests      PASS
clang-format                     PASS
clang-tidy                       PASS
ASan + UBSan                     PASS
Phase 1 evidence collected       PASS
GitHub Actions                   PASS
Documentation written/read       PASS
Git tag v0.2-cpp-core            created after CI is green
```

The evidence rule remains:

```text
Code works
+
Test exists
+
Evidence exists
+
Documentation exists
=
Phase complete
```

---

## 0. Start from the completed Phase 0 repository

Your current repository should already be at:

```text
v0.1-project-skeleton
```

Before changing anything:

```bash
cd ~/Documents/linux-voip-softphone

git status
git log --oneline --decorate --graph --all
git tag --list
```

Expected conditions:

```text
working tree clean
origin/main up to date
v0.1-project-skeleton exists
```

Create a working branch:

```bash
git switch -c phase/1-modern-cpp
```

Do not move or rewrite `v0.1-project-skeleton`.

---

## 1. Understand the Phase 1 target

The roadmap requires practical use of:

```text
RAII
smart pointer
move semantics
const correctness
enum class
std::optional
std::variant
STL containers
interfaces
dependency injection cơ bản
```

The target files in this package are:

```text
labs/cpp/
├── raii/
├── smart-pointers/
└── move-semantics/

src/core/
├── BuildInfo.cpp
├── BuildInfo.hpp
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

The package is a **reference target state**. For learning value, do not merely copy files and push
them without reading them. Implement/read each checkpoint below, run it, explain it in your own
words, and commit in small steps.

---

# Day 1 — RAII and resource ownership

## 2. Study the RAII wrapper

Open:

```bash
less labs/cpp/raii/FileDescriptor.hpp
less labs/cpp/raii/main.cpp
```

The important shape is the same idea shown in the roadmap:

```cpp
class FileDescriptor {
public:
    explicit FileDescriptor(int fd);
    ~FileDescriptor();

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
};
```

The Phase 1 implementation goes one step further and also supports move ownership.

You must be able to explain:

```text
Why does the destructor close the fd?
Why is copying deleted?
What would double-close mean?
Why is moving safe here?
What does release() do?
Why is this RAII rather than manual cleanup?
```

Build:

```bash
cmake --preset debug
cmake --build --preset debug -j
```

Run the RAII lab:

```bash
./build/debug/labs/cpp/raii/lab_raii
```

Expected behavior resembles:

```text
first owns fd: true
first after move: false
second after move: true
fd closed by destructor: true
```

The exact file descriptor number is deliberately not used as evidence because it may vary.

### Exercise

Temporarily comment out the destructor's `reset()` call, rebuild, and observe that the final
`fd closed by destructor` check becomes false. Restore the code immediately afterward.

This demonstrates the purpose of RAII using runtime behavior rather than a definition alone.

Suggested commit after you understand and restore the correct implementation:

```bash
git add labs/cpp/raii CMakeLists.txt labs/CMakeLists.txt labs/cpp/CMakeLists.txt
git commit -m "lab(cpp): add RAII file descriptor wrapper"
```

---

# Day 2 — Smart pointers and move semantics

## 3. Smart pointer lab

Read:

```bash
less labs/cpp/smart-pointers/main.cpp
```

Run:

```bash
./build/debug/labs/cpp/smart-pointers/lab_smart_pointers
```

The lab demonstrates three ownership roles:

```text
std::unique_ptr  -> exclusive ownership
std::shared_ptr  -> shared ownership
std::weak_ptr    -> non-owning observation of shared state
```

You must explain why `weak_ptr` does not keep `AudioDevice` alive and what `lock()` does.

### Exercise

Record the `shared_ptr::use_count()` during and after `CallContext` lifetime. Explain why the count
changes. Do not use `use_count()` as production ownership logic; here it is only an educational
observation.

Suggested commit:

```bash
git add labs/cpp/smart-pointers
git commit -m "lab(cpp): demonstrate smart pointer ownership"
```

## 4. Move semantics lab

Read:

```bash
less labs/cpp/move-semantics/main.cpp
```

Run:

```bash
./build/debug/labs/cpp/move-semantics/lab_move_semantics
```

Expected concept:

```text
original owns 1024 bytes
move transfers ownership
moved-from object remains valid with size 0 in this implementation
move assignment transfers ownership again
```

You must explain:

```text
copy constructor vs move constructor
copy assignment vs move assignment
std::move does not move by itself; it enables move overload selection
why noexcept matters for move operations
valid-but-moved-from state
```

Suggested commit:

```bash
git add labs/cpp/move-semantics
git commit -m "lab(cpp): demonstrate move-only buffer ownership"
```

---

# Day 3 — Production `src/core/`

## 5. Implement `CallState` using `enum class`

Read:

```bash
less src/core/CallState.hpp
less src/core/CallState.cpp
```

The required state set from the roadmap is:

```cpp
enum class CallState {
    Idle,
    Registering,
    Ready,
    Calling,
    Ringing,
    Connected,
    Terminating,
    Failed
};
```

This project also provides:

```text
to_string(CallState)
can_transition(StateTransition{from, to})
```

Run just the state tests after building:

```bash
ctest --preset debug -R CallStateTest --output-on-failure
```

Study invalid transitions carefully. For example:

```text
Idle -> Connected        invalid
Ready -> Connected       invalid
Connected -> Calling     invalid
```

Do not memorize the table. Explain why each transition is or is not legal in the lifecycle.

Suggested commit:

```bash
git add src/core/CallState.* tests/unit/test_call_state.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(core): add explicit call state transitions"
```

## 6. `std::variant` events

Read:

```bash
less src/core/Event.hpp
```

The event model uses a closed, type-safe union:

```text
RegisterRequested
RegistrationSucceeded
RegistrationFailed
OutgoingCallRequested
IncomingCallReceived
RemoteRinging
CallConnected
HangupRequested
CallEnded
CallFailed
ResetRequested
```

`std::variant` means an `Event` contains exactly one of those alternatives at a time.

Then inspect:

```bash
less src/core/CallStateMachine.cpp
```

Find the `std::visit` call and explain how it replaces unsafe manual type tags or `void*` payloads.

## 7. `std::optional` and STL containers

Inspect:

```bash
less src/core/CallStateMachine.hpp
```

Find:

```text
std::optional<std::string> peer_
std::optional<std::string> last_error_
std::vector<CallState> history_
```

Explain:

```text
Why optional is better than an empty-string sentinel here
Why vector is suitable for state history
What operations would make another container more appropriate
```

## 8. Interface + constructor dependency injection

Read:

```bash
less src/core/StateChangeSink.hpp
```

`StateChangeSink` is an interface. `CallStateMachine` receives it from outside:

```cpp
explicit CallStateMachine(StateChangeSink& sink);
```

This is basic constructor dependency injection.

The core state machine therefore does not need to know whether the consumer is:

```text
console logging
Qt UI
unit-test fake
future diagnostics system
```

Read `tests/unit/test_call_state_machine.cpp` and identify `RecordingSink`. Explain why injecting a
fake dependency makes the core easier to test.

Suggested commit:

```bash
git add src/core/Event.hpp src/core/StateChangeSink.* src/core/CallStateMachine.* src/core/Types.hpp
git add tests/unit/test_call_state_machine.cpp src/app/main.cpp src/core/BuildInfo.cpp
git commit -m "feat(core): add event-driven call state machine"
```

---

# Day 4 — Tests, const correctness, and code review

## 9. Run all unit tests

```bash
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug --output-on-failure
```

You should see all tests pass, including invalid state transition coverage.

Run the application demo:

```bash
./build/debug/src/linux-voip-softphone
```

Expected transition flow:

```text
Idle -> Registering
Registering -> Ready
Ready -> Calling
Calling -> Ringing
Ringing -> Connected
Connected -> Terminating
Terminating -> Ready
```

## 10. Review const correctness

Search for const usage:

```bash
grep -R "const" -n src/core labs/cpp tests/unit
```

Be able to distinguish:

```text
const local/value
const reference parameter
const member function
const return reference
constexpr (not required by this phase)
```

Ask yourself for every getter: should it mutate state? If not, it should normally be a const member
function.

## 11. Format check

```bash
find src tests labs -type f \( -name '*.cpp' -o -name '*.hpp' \) \
    -print0 | xargs -0 clang-format --dry-run --Werror

echo $?
```

Expected:

```text
0
```

If it fails:

```bash
find src tests labs -type f \( -name '*.cpp' -o -name '*.hpp' \) \
    -print0 | xargs -0 clang-format -i
```

Then inspect the diff rather than formatting blindly:

```bash
git diff
```

## 12. clang-tidy

```bash
./scripts/run_clang_tidy.sh
```

Do not ignore a warning just to make CI green. Read the diagnostic, decide whether it represents a
real issue, and only suppress checks when you can explain why.

## 13. Sanitizers

```bash
./scripts/run_sanitizers.sh
```

Phase 1 must remain clean under:

```text
AddressSanitizer
UndefinedBehaviorSanitizer
```

Remember:

```text
compiler warning != clang-tidy != sanitizer
```

They detect different classes of problems.

---

# Day 5 — Documentation and evidence

## 14. Read and improve the architecture document

Open:

```bash
less docs/architecture/call-state-machine.md
```

Do not leave it as text you cannot defend. Edit wording where useful so it reflects your own
understanding after running the code.

The roadmap specifically requires this artifact:

```text
docs/architecture/call-state-machine.md
```

and this unit test:

```text
tests/unit/test_call_state.cpp
```

## 15. Collect Phase 1 evidence

Run:

```bash
./scripts/collect_phase1_evidence.sh
```

Then inspect:

```bash
tree evidence/01-cpp
```

Expected artifacts:

```text
evidence/01-cpp/
├── README.md
├── build.log
├── clang-tidy.log
├── format-check.log
├── git-history.txt
├── move-semantics-demo.log
├── raii-demo.log
├── sanitizers.log
├── smart-pointers-demo.log
├── toolchain.txt
└── unit-tests.log
```

These files must come from your own machine. They are evidence, not template content.

Run the one-command verification as a final local gate:

```bash
./scripts/verify_phase1.sh
```

---

# Day 6 — Interview review, Git history, CI, milestone

## 16. Interview topics required by the roadmap

You must be able to explain, using this repository as evidence:

```text
unique_ptr vs shared_ptr
RAII giải quyết vấn đề gì
copy vs move
vector vs list
map vs unordered_map
enum class tốt hơn enum ở đâu
```

Use:

```bash
less docs/interview/phase1-questions.md
```

Then answer those questions aloud without reading the file. If you cannot explain one, Phase 1 is
not done yet.

## 17. Final Git review

```bash
git status
git diff --check
git log --oneline --decorate --graph --all
```

Keep meaningful history. A good Phase 1 history might contain commits like:

```text
lab(cpp): add RAII file descriptor wrapper
lab(cpp): demonstrate smart pointer ownership
lab(cpp): demonstrate move-only buffer ownership
feat(core): add explicit call state transitions
feat(core): add event-driven call state machine
test(core): cover invalid call state transitions
docs(core): document call state machine decisions
```

The exact commit split can differ. Do not manufacture history after the fact just to imitate this
list.

## 18. Push your Phase 1 branch

Because your repository uses the `haikevins` SSH alias, the existing `origin` should remain:

```text
git@github-haikevins:haikevins/linux-voip-softphone.git
```

Verify:

```bash
git remote -v
```

Push:

```bash
git push -u origin phase/1-modern-cpp
```

Check all GitHub Actions. Build, Tests and Sanitizers, and Static Analysis must be green.

After review, merge into `main` using your normal GitHub or local workflow. Then update local main:

```bash
git switch main
git pull --ff-only origin main
```

Run the final verification one more time on `main`:

```bash
./scripts/verify_phase1.sh
```

## 19. Create the Phase 1 milestone tag

Only after local verification and CI are green:

```bash
git tag -a v0.2-cpp-core \
    -m "Phase 1: Modern C++ foundation and call state core"

git push origin v0.2-cpp-core
```

Confirm:

```bash
git log --oneline --decorate --graph --all
git tag --list
```

Do not move `v0.2-cpp-core` after Phase 1 is accepted. It becomes the historical snapshot for the
milestone.

---

# Final self-check

Before moving to Phase 2, answer **yes** to all of these:

```text
[ ] I can explain why FileDescriptor is non-copyable.
[ ] I can explain why its move constructor leaves the source non-owning.
[ ] I know when unique_ptr is preferable to shared_ptr.
[ ] I can explain weak_ptr without saying only "it prevents memory leaks".
[ ] I understand that std::move is a cast enabling move semantics.
[ ] I can explain why CallState is enum class.
[ ] I can explain optional peer/error values.
[ ] I can explain Event as std::variant and std::visit dispatch.
[ ] I can explain why vector is used for transition history.
[ ] I can compare vector/list and map/unordered_map.
[ ] I can explain StateChangeSink as an interface.
[ ] I can explain constructor dependency injection in CallStateMachine.
[ ] I can point to tests for invalid transitions.
[ ] clang-format, clang-tidy, ASan, UBSan and tests all pass.
[ ] GitHub Actions are green.
[ ] evidence/01-cpp contains outputs generated on my Ubuntu machine.
[ ] v0.2-cpp-core points at the completed Phase 1 commit on main.
```

When every item is true, proceed to Phase 2 — Linux System Programming.
