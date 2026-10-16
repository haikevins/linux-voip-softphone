# Phase 9 Architecture — Debugging and Memory Diagnostics

## Status

In progress — target milestone `v0.9-diagnostics`.

## Purpose

Define a repeatable native-debugging architecture for controlled crashes, saved-core analysis, sanitizer-driven memory investigation, and post-fix regression while keeping intentional undefined behavior out of the production softphone.

## Scope

This architecture covers:

- isolation of deterministic debugging scenarios in `tools/crash_generator`;
- GDB-controlled `SIGSEGV` handling and explicit core-file generation;
- fresh-process analysis of a saved core;
- the historical heap use-after-free scenario and its lifetime repair;
- ASan+UBSan verification of the repaired path;
- safe Phase 9 CTest coverage and aggregate verification;
- the boundary between disposable diagnostic artifacts and committed evidence.

It does not redefine SIP/PJSUA2 ownership, the Qt/QML model, RTP/media behavior, the Phase 8 worker architecture, Linux system-wide crash collection policy, or production crash telemetry.

## Architecture model

### Diagnostic isolation

The crash generator is a separate executable target:

```text
linux-voip-softphone production executables
    -> normal application behavior
    -> no intentional Phase 9 UB injected

build/.../tools/crash_generator/crash_generator
    ├── --help
    ├── null-deref
    └── use-after-free
```

The name `use-after-free` is retained as a historical regression scenario. In the current source it exercises the repaired lifetime path and must exit successfully.

### Saved-core path

The core-dump workflow is:

```text
capture-core-dump.sh
    -> cmake --preset debug
    -> build crash_generator
    -> GDB run crash_generator null-deref
       -> handle SIGSEGV stop print nopass
       -> process stops at intentional fault
       -> generate-core-file build/phase9-core/crash_generator.core
       -> live backtrace saved
    -> second GDB process
       -> open matching crash_generator + saved core
       -> bt
       -> validate SIGSEGV
       -> validate triggerNullDereference
       -> validate main
```

The script deliberately asks GDB to create the core. Linux machines may redirect kernel crash handling through `systemd-coredump`, Apport, or another `core_pattern` handler; explicit GDB generation makes the lab independent of that host policy while still keeping `ulimit -c unlimited` as the conventional core-dump prerequisite.

### Memory-lifetime path

The historical defective shape was:

```text
owner = std::make_unique<int>(42)
raw alias = owner.get()
owner.reset()
raw alias is now dangling
read through dangling alias
    -> heap-use-after-free
    -> AddressSanitizer diagnostic
```

The repaired shape is:

```text
owner = std::make_unique<int>(42)
observer = owner.get()
read observer while owner is alive
validate value
owner.reset()
observer = nullptr
no further dereference
    -> normal exit
    -> no ASan/UBSan diagnostic
```

`std::unique_ptr` establishes ownership, but the raw pointer is still non-owning. Safety comes from the lifetime rule that every dereference occurs before owner destruction, not merely from the presence of a smart pointer somewhere in the function.

### Verification layering

Phase 9 uses different tools for different failure classes:

```text
CTest
    -> safe CLI/regression behavior

GDB + saved core
    -> signal location
    -> stack reconstruction
    -> post-mortem inspection workflow

ASan + UBSan
    -> dynamic invalid-memory/undefined-behavior detection
    -> post-fix clean run

Phase 9 aggregate verifier
    -> inherited Phase 8 gate
    -> safe Phase 9 CTest
    -> saved-core workflow
    -> ASan+UBSan lifetime verification
    -> sanitizer CTest regression
```

No one layer replaces the others. A backtrace identifies the failing execution path but does not by itself prove ownership correctness. A clean ASan run covers only the exercised paths. CTest protects the expected fixed behavior but does not produce post-mortem crash analysis.

### Artifact boundary

Runtime artifacts are intentionally separated from source/evidence:

```text
build/phase9-core/
    crash_generator.core
    gdb-live.txt
    gdb-core-backtrace.txt

build/phase9-asan/
    asan-use-after-free.txt
```

`build/` remains disposable and ignored by Git. The closure commit records reviewed summaries under `evidence/phase9/` instead of storing a machine-specific binary core.

## Runtime and lifecycle

### `null-deref` scenario

`triggerNullDereference()` prints a marker, flushes stdout, creates a null address deliberately, and performs the invalid write. Under the Phase 9 GDB script, GDB is configured to stop and not pass `SIGSEGV` onward immediately, allowing a deterministic core to be generated from the stopped inferior.

This scenario remains intentionally invalid. It exists only to exercise debugger mechanics and is not part of a production execution path.

### Core capture and reopen

`capture-core-dump.sh` first builds the debug target, prepares `build/phase9-core/`, removes stale outputs, and launches GDB in batch mode. Once the inferior stops on the intentional fault, GDB writes `crash_generator.core` and records a live backtrace.

A second independent GDB invocation reopens the matching executable and core. The script fails unless the saved-core transcript contains the signal and expected function names. This second invocation matters because it verifies post-mortem use of the persisted core rather than only inspecting a still-running debugger session.

### Historical UAF reproduction

The historical Phase 9 revision kept a raw pointer returned by `unique_ptr::get()`, reset the owner, and then read through the stale pointer. The helper was marked `noinline` on GCC/Clang so the invalid read remained visible as a distinct frame in the sanitizer stack.

That faulty revision is retained in Git history for the debugging narrative; it is not kept in the current executable behavior.

### Lifetime repair

The current `verifyRepairedLifetime()` reads through the observer while the `unique_ptr` still owns the allocation, checks the expected value, resets the owner, clears the observer, and exits successfully.

The repair is intentionally about ordering and ownership semantics. Nulling the observer after destruction is defensive state cleanup, but the critical property is that no dereference occurs after `owner.reset()`.

### Sanitizer execution

`verify-asan-use-after-free.sh` removes `build/sanitizers` before configuring the sanitizer preset. This prevents Ninja from accepting stale crash-generator objects when a patch was applied with checksum-mode rsync while preserving source mtimes.

The script runs the repaired path with strict ASan/UBSan halt options, requires exit status zero, rejects AddressSanitizer/UBSan error markers, and requires the explicit repaired-lifetime success marker.

### Aggregate execution

`scripts/phase9/verify.sh` first executes the inherited Phase 8 verifier, then runs the Phase 9-labeled safe tests, saved-core workflow, repaired ASan+UBSan check, and sanitizer Phase 9 CTest regression.

Intentional crash artifacts and sanitizer logs stay under `build/`; running the verifier must not turn them into tracked source files.

## Verification strategy

The Phase 9 verification model checks both failure reproduction mechanics and fixed behavior:

- `Phase9CrashGeneratorHelpTest` verifies the diagnostic CLI remains discoverable without crashing.
- `Phase9RepairedLifetimeRegressionTest` runs the historical scenario name as a safe fixed-path regression.
- `capture-core-dump.sh` verifies debug build, controlled `SIGSEGV`, explicit core creation, and fresh saved-core backtrace analysis.
- `verify-asan-use-after-free.sh` verifies a fresh ASan+UBSan build and rejects any remaining sanitizer diagnostic on the repaired path.
- `scripts/phase9/verify.sh` inherits all Phase 8 checks before adding the Phase 9 gates.
- CI runs the safe Phase 9 tests in sanitizer-oriented jobs; the intentional null crash remains in the explicit GDB workflow rather than a normal test job.

The latest pre-documentation PJSIP debug run passed 141/141 tests, and the repaired-lifetime regression passed 20 consecutive repetitions. These observations support stability of the current path but do not replace broader sanitizer or post-mortem analysis.

## Design rationale

### Why isolate intentional faults in a tool?

The portfolio goal is to demonstrate debugging skill without weakening the real softphone. A dedicated target makes the failure deterministic, reviewable, and easy to exclude from production runtime behavior.

### Why keep the null dereference after the UAF is repaired?

They serve different goals. `null-deref` is the stable trigger for GDB/core mechanics. The UAF scenario demonstrates memory-lifetime diagnosis and repair. Removing the null fault would eliminate the deterministic crash lab rather than improve production safety.

### Why explicitly generate the core in GDB?

System-wide Linux core handling varies. Depending only on `/proc/sys/kernel/core_pattern` would make the exercise sensitive to distro services and permissions. GDB-generated cores preserve the post-mortem workflow while making the repository verifier repeatable.

### Why analyze the core in a second GDB process?

A live debugger already has process state. Reopening the saved core with the executable verifies that the persisted artifact actually contains enough state for post-mortem backtrace recovery.

### Why is `std::unique_ptr` not sufficient by itself?

Smart pointers solve ownership, not every aliasing mistake. A raw pointer obtained with `.get()` does not extend lifetime. If code dereferences that alias after the owner releases the object, the program still has a use-after-free.

### Why retain the `use-after-free` scenario name after fixing it?

The scenario name records the defect class being guarded against. Changing the CLI to a generic success case would weaken the connection between the historical failing revision, the repair, and the regression test.

### Why not register `null-deref` as a normal CTest success case?

Ordinary regression tests should succeed when the program behaves correctly. The intentional crash requires specialized debugger control and post-mortem assertions. Keeping it in a dedicated script avoids teaching the general test suite that a raw crash is a normal successful outcome.

### Why force a fresh sanitizer build?

A sanitizer conclusion is only meaningful when the tested binary reflects current source. Removing the disposable sanitizer build tree prevents preserved mtimes from causing Ninja to reuse an older object compiled from the historical failing or previously repaired implementation.

### What does a clean ASan run prove?

It shows that the exercised repaired path did not trigger the enabled sanitizer checks in that run. It does not prove the whole softphone is memory-safe, cover every interleaving, or replace code review and ownership reasoning.

## Phase boundary

Phase 9 adds diagnostic tooling and verification around controlled native failures. It does not alter the production SIP, RTP, GUI, or multithreading architecture established in earlier phases.

Intentional undefined behavior is confined to the standalone `null-deref` lab scenario. The historical UAF remains in Git history as the before-state; the current branch keeps only the repaired safe path under regression. Core files and raw diagnostic transcripts remain disposable build artifacts, while reviewed summaries are reserved for Phase 9 evidence closure.

Phase 10 should proceed as a separate RTSP mini-lab and must not reuse the Phase 9 crash tool as an excuse to couple unrelated protocol work to the softphone.

## References

- `docs/phase9/README.md`
- `docs/phase9/core-dump-analysis.md`
- `docs/interview/phase9-questions.md`
- `docs/architecture/threading-model.md`
- `tools/crash_generator/main.cpp`
- `tools/crash_generator/CMakeLists.txt`
- `scripts/phase9/capture-core-dump.sh`
- `scripts/phase9/verify-asan-use-after-free.sh`
- `scripts/phase9/verify.sh`
- `CMakeLists.txt`
- `.github/workflows/test.yml`
