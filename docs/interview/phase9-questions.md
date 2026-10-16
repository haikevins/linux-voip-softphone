# Phase 9 Interview Review — Debugging and Memory Diagnostics

Use the repository implementation, documentation, tests, and evidence as the basis for the answers. Explain the debugging decision, observed failure mode, and repaired behavior; do not memorize isolated tool definitions.

## Review scope

- controlled lab-only crashes and failure isolation
- Linux core dumps and GDB post-mortem analysis
- heap use-after-free diagnosis and lifetime repair
- `std::unique_ptr` ownership versus raw non-owning aliases
- AddressSanitizer and UndefinedBehaviorSanitizer
- safe regression design and stale-build risks
- disposable diagnostics versus committed evidence

## Questions and answers

### Controlled crash design

#### Why create a dedicated `crash_generator` instead of crashing the softphone itself?

A debugging exercise should not weaken production behavior. The standalone tool gives a deterministic fault with a tiny call path, so GDB and sanitizer behavior can be studied without mixing the result with SIP registration, Qt event loops, PJSUA2 callbacks, audio devices, or Phase 8 threading.

It also makes the safety boundary obvious in code review: intentional undefined behavior belongs to a lab executable, not the normal application binaries.

#### What scenarios does the tool expose now?

It exposes `--help`, `null-deref`, and `use-after-free`.

`null-deref` intentionally crashes for the GDB/core exercise. `use-after-free` is now a historical scenario name: the current implementation exercises the repaired lifetime path and must complete successfully. The actual failing UAF implementation remains available in Git history rather than in current runtime behavior.

#### Why keep an intentionally crashing path after fixing the memory bug?

The null dereference and the UAF serve different lessons. The null dereference is a stable trigger for debugger/core mechanics. The UAF demonstrates ownership failure, sanitizer detection, repair, and regression.

Keeping the null crash does not mean leaving a production bug unfixed because the crash tool is not on the production softphone path.

### GDB and core dumps

#### What is a core dump?

A core dump is a snapshot of relevant process state around a failure or debugger-controlled stop. Combined with the matching executable and debug information, it can be used for post-mortem inspection of stacks, registers, variables, and memory mappings without the original process still running.

#### Why run a debug build for the core-dump exercise?

Debug symbols make function names, source locations, and variables recoverable. An optimized stripped binary may still yield addresses and some symbols, but the investigation becomes less informative and compiler optimization can make source-level state harder to reconstruct.

#### Why does the script call `ulimit -c unlimited` if GDB generates the core explicitly?

`ulimit -c unlimited` documents the conventional Linux prerequisite and keeps the process environment compatible with core generation. The workflow then asks GDB to write the core directly so distro-specific kernel `core_pattern` handlers do not decide where the lab artifact goes.

#### Why not depend on `/proc/sys/kernel/core_pattern`?

Different distributions may route crashes to `systemd-coredump`, Apport, another service, or a pipe command. Permissions and retention policy also differ. That would make the repository exercise host-specific.

Using GDB `generate-core-file` keeps the output path under the repository `build/` tree and makes the same verification logic portable across development hosts with GDB.

#### What does `handle SIGSEGV stop print nopass` accomplish?

It tells GDB to stop when the inferior receives `SIGSEGV`, print the signal, and not immediately pass it back to the program. That leaves the process stopped at the fault so the script can generate a core and collect a backtrace deterministically.

#### Why reopen the saved core in a second GDB invocation?

A live GDB session already has direct access to the inferior. Reopening the core with a fresh debugger proves the persisted core can actually support post-mortem analysis.

The script therefore validates the saved-core transcript, not only the live crash session.

#### What markers does the Phase 9 saved-core check require?

The transcript must show `SIGSEGV`, `triggerNullDereference`, and `main`.

Those markers establish that the expected controlled fault occurred and that the call path is reconstructable. They do not prove every memory value in the process is meaningful or that a production root cause has been identified.

#### Why must the executable match the core?

Addresses in the core correspond to a particular binary layout. Rebuilding or using a different executable can change code addresses, symbols, and optimization decisions. Reliable post-mortem analysis therefore needs the executable/debug information that corresponds to the captured process image.

### Heap use-after-free and ownership

#### What was the historical UAF bug in the lab?

The code created an owning `std::unique_ptr<int>`, stored `owner.get()` in a raw pointer, called `owner.reset()`, and then dereferenced the raw pointer.

After `reset()`, the allocation had been freed. The raw pointer still contained an address, but it no longer referred to a live object. Dereferencing it was a heap use-after-free.

#### Why did `std::unique_ptr` not prevent the bug automatically?

`std::unique_ptr` controls ownership and destruction. A raw alias returned by `.get()` is non-owning and does not participate in lifetime management.

The programmer still has to ensure the alias is used only while the owner keeps the object alive. Smart ownership reduces ambiguity, but it cannot make an incorrectly retained raw observer safe.

#### What is the actual fix?

The current path performs the read while the owner is still alive, validates the value, then resets the owner and clears the observer. No dereference occurs after destruction.

The important fix is lifetime ordering. Assigning `nullptr` afterward makes the invalid state explicit, but nulling a pointer after an already-invalid dereference would not repair anything.

#### Why keep a raw observer at all in such a small example?

The purpose is educational: many real C++ APIs expose non-owning pointers or references into objects owned elsewhere. The lab demonstrates that ownership and observation are separate concepts and that non-owning access needs an explicit lifetime contract.

### Sanitizers

#### What does AddressSanitizer detect here?

ASan instruments memory accesses and allocator behavior so invalid accesses such as heap use-after-free can be reported at runtime when the exercised path triggers them. The historical Phase 9 revision intentionally produced that diagnostic.

The repaired verifier now expects the same logical scenario to exit normally with no AddressSanitizer error.

#### Why run UndefinedBehaviorSanitizer too?

The repository sanitizer preset combines complementary dynamic checks. UBSan can catch classes of undefined behavior that are not specifically heap-address violations. The repaired script rejects UBSan runtime-error markers as well as ASan diagnostics.

#### Does an ASan-clean run prove the program is memory-safe?

No. Dynamic sanitizers observe executed paths and supported defect classes. Unexecuted branches, rare timing conditions, unsupported errors, and environment-specific behavior can remain.

A clean run is strong evidence for the exercised path, not a mathematical proof of global safety.

#### Why is leak detection disabled in the focused lifetime script?

The script is focused on the historical use-after-free path and its immediate repair. Setting `detect_leaks=0` avoids turning unrelated platform/library leak reporting into the acceptance criterion for this focused regression. Leak analysis can be a separate concern when required.

#### Why use `halt_on_error=1` and `abort_on_error=1`?

The verifier should fail immediately if ASan sees a defect. Continuing after a sanitizer error can produce secondary failures and make automation ambiguous. Strict termination converts the first sanitizer finding into a clear failed gate.

### Regression and build correctness

#### Why does the repaired scenario still use the CLI name `use-after-free`?

The name identifies the defect class the regression protects against. It connects the historical failing revision, the ownership fix, and the current safe test.

A regression test is often more useful when its name remembers what must never come back.

#### Why is `null-deref` not a normal CTest case?

The path is supposed to crash. Treating a raw process crash as ordinary CTest success would blur the difference between correct program behavior and a debugger exercise.

The repository instead gives that fault a specialized GDB script with explicit expectations, while CTest runs only safe behaviors.

#### What are the two safe Phase 9 CTest cases?

`Phase9CrashGeneratorHelpTest` checks the tool CLI without invoking UB. `Phase9RepairedLifetimeRegressionTest` runs the repaired historical UAF scenario and expects normal completion.

They are labeled `phase9`; the repaired case is also labeled `regression`.

#### Why does the ASan verification remove the sanitizer build tree first?

Patch application used checksum-mode rsync while preserving mtimes. Build systems such as Ninja primarily use timestamps to decide whether an object is stale. A newly copied source can therefore contain different bytes without looking newer than an existing object.

Deleting the disposable sanitizer build tree guarantees the tested binary is recompiled from current source before drawing a sanitizer conclusion.

#### What does the aggregate Phase 9 verifier inherit from Phase 8?

It runs `scripts/phase8/verify.sh` first. That keeps all previously accepted build, regression, GUI, worker, queue, stress, and sanitizer-build expectations in force before adding Phase 9-specific diagnostics.

Phase 9 is additive; it does not replace earlier gates.

### Debugging strategy and interpretation

#### How would you use GDB and ASan together on a real C++ defect?

Use the tool that best exposes the symptom. ASan is excellent when the defect is an invalid memory access that can be reproduced under instrumentation. GDB is useful for controlling execution, inspecting state, following call stacks, and analyzing crashes after the fact through a core.

A practical sequence is: reproduce, get a precise failure location, reason about ownership/lifetime, implement the smallest correct repair, then retain an automated regression that exercises the repaired behavior.

#### Is the top frame of a backtrace always the root cause?

No. The top frame is where the process faulted, which can be downstream of the actual bug. A dangling pointer may be created much earlier and only crash when later dereferenced.

Root-cause analysis has to connect the crash site to ownership history, state transitions, and object lifetime rather than simply renaming the top frame as the cause.

#### Why avoid committing the binary core file?

Core files are large, machine-specific, tied to a particular executable build, and can contain arbitrary process memory. In a real application that memory could include credentials or other sensitive runtime data.

The repository keeps the core disposable under `build/` and commits only curated, sanitized evidence summaries.

#### What is the most important Phase 9 boundary?

Intentional failure is a laboratory technique, not production architecture. The null dereference stays isolated in the crash tool; the historical UAF stays in Git history; the current branch keeps the repaired safe path.

The production SIP/Qt/threading model remains the Phase 8 design.

## Phase boundary

Phase 9 demonstrates a complete native-debugging loop: deterministic failure, post-mortem core analysis, sanitizer-based memory diagnosis, ownership/lifetime repair, and automated regression.

It does not add production crash reporting, remote symbol servers, fuzzing, heap profiling, leak-tracking policy, or a new threading model. It also does not turn lab-only undefined behavior into application behavior.

Phase 10 is a separate RTSP mini-lab and should not modify these diagnostic boundaries unless a later requirement explicitly needs shared tooling.

## References

- `docs/phase9/README.md`
- `docs/phase9/core-dump-analysis.md`
- `docs/architecture/debugging-diagnostics.md`
- `docs/architecture/threading-model.md`
- `tools/crash_generator/main.cpp`
- `scripts/phase9/capture-core-dump.sh`
- `scripts/phase9/verify-asan-use-after-free.sh`
- `scripts/phase9/verify.sh`
- `CMakeLists.txt`
