# Phase 9 Core-Dump and Lifetime Analysis

This note is the Phase 9 debugging report for the controlled native failures. It documents the reproducible investigation contract rather than storing a raw binary core or a full host-specific debugger transcript.

## Controlled crash

### Symptom

Running the lab-only `null-deref` scenario under the Phase 9 GDB workflow reaches an intentional invalid write and stops with `SIGSEGV`.

```bash
./scripts/phase9/capture-core-dump.sh
```

The script builds the debug `crash_generator`, configures GDB to stop on `SIGSEGV`, and runs:

```text
crash_generator null-deref
```

### Backtrace contract

The saved-core analysis must contain:

```text
SIGSEGV
triggerNullDereference
main
```

The exact instruction address, line number, and surrounding system frames are build/toolchain dependent and are therefore not treated as repository-stable assertions.

### Root cause

This crash is intentional. `triggerNullDereference()` constructs address zero and performs a write through that invalid pointer so the repository has a deterministic native crash for debugger training.

It is not a production softphone defect and is not reachable from the production applications.

### Core capture

The workflow does not rely on the host kernel's configured `core_pattern`. After GDB stops the inferior on the fault, it explicitly runs `generate-core-file` and writes:

```text
build/phase9-core/crash_generator.core
```

A second GDB process opens the matching executable and saved core and runs `bt`. This verifies post-mortem analysis from the persisted core rather than only the live debugging session.

### Verification

The script fails unless:

- the debug crash-generator binary exists;
- the generated core is non-empty;
- fresh saved-core analysis reports `SIGSEGV`;
- the backtrace includes `triggerNullDereference`;
- the backtrace includes `main`.

On success it prints:

```text
Phase 9 core-dump/GDB verification: PASS
```

## Historical heap use-after-free

### Symptom

The historical Phase 9 UAF revision created a heap object with `std::unique_ptr<int>`, saved a raw alias, destroyed the owner, and then read through the stale alias.

Conceptually:

```text
owner owns allocation
    -> raw alias observes allocation
    -> owner.reset() frees allocation
    -> raw alias becomes dangling
    -> dereference dangling alias
    -> heap-use-after-free
```

Under the sanitizer build, AddressSanitizer identified the invalid heap access. The intentionally failing implementation is preserved by Git history instead of remaining in the current source.

### Root cause

The defect was a lifetime-ordering error, not a failure of `std::unique_ptr` itself.

`unique_ptr::get()` returns a non-owning raw pointer. That pointer does not keep the allocation alive. Calling `owner.reset()` ends the object lifetime, so any later dereference through the alias is invalid.

### Fix

The repaired path keeps the read inside the ownership window:

```text
owner = std::make_unique<int>(42)
observer = owner.get()
read observer
validate 42
owner.reset()
observer = nullptr
no further access
```

The key change is that the dereference occurs before destruction. Clearing the observer afterward makes the post-destruction state explicit but is not a substitute for correct ordering.

### Post-fix verification

Run:

```bash
./scripts/phase9/verify-asan-use-after-free.sh
```

The script performs a fresh sanitizer build, executes:

```text
crash_generator use-after-free
```

and requires all of the following:

- exit status is zero;
- no `ERROR: AddressSanitizer` marker appears;
- no UBSan `runtime error:` marker appears;
- no `heap-use-after-free` marker appears;
- the repaired-lifetime success message appears.

On success it prints:

```text
Phase 9 ASan lifetime-fix verification: PASS
```

The current CTest regression also runs the repaired path as `Phase9RepairedLifetimeRegressionTest` so the lifetime mistake cannot silently return without breaking ordinary test jobs.

## Current verification observation

Before this documentation commit, the PJSIP debug suite was rerun successfully with:

```text
141/141 tests passed
```

Both Phase 9 tests passed in that full suite, and `Phase9RepairedLifetimeRegressionTest` also passed 20 consecutive repetitions. These are observations from the tested build and are not used as hard-coded total-suite requirements.

## Artifact and security boundary

The following are disposable build artifacts and should not be committed:

```text
build/phase9-core/crash_generator.core
build/phase9-core/gdb-live.txt
build/phase9-core/gdb-core-backtrace.txt
build/phase9-asan/asan-use-after-free.txt
```

A core can contain arbitrary process memory. Although the current crash generator contains no SIP credentials, the repository policy still treats binary cores and raw diagnostic dumps as transient artifacts. Phase 9 closure should retain only the concise evidence needed to demonstrate the result.

## Reproduction summary

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

./scripts/phase9/capture-core-dump.sh
./scripts/phase9/verify-asan-use-after-free.sh
./scripts/phase9/verify.sh
```

The first command demonstrates deterministic post-mortem crash analysis. The second demonstrates the repaired heap lifetime under ASan+UBSan. The aggregate verifier preserves all inherited Phase 8 gates and adds both Phase 9 diagnostic paths.
