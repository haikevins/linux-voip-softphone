# Phase 9 — Debugging, Memory Diagnostics, and Core Dumps

## Status

**Completed** — milestone `v0.9-diagnostics`.

Phase 9 closes with the lab-only crash/core workflow, repaired lifetime regression, sanitizer-backed verification, CI coverage, architecture/interview documentation, and curated sanitized evidence in place. The annotated tag is created only after review, merge to `main`, and verification of the merged closure state.

## Objective

Build a reproducible debugging workflow for native C++ failures: create controlled lab-only faults, capture and inspect a saved core with GDB, reproduce a heap lifetime defect under AddressSanitizer, repair the lifetime ordering, and keep the repaired path under automated regression without weakening the Phase 8 threading boundary.

## Scope

Included in this phase:

- a standalone `crash_generator` target under `tools/` for deterministic debugging exercises;
- an intentional `null-deref` scenario that produces a controlled `SIGSEGV` only in the lab tool;
- reproducible GDB capture that explicitly writes a core file instead of depending on host-specific `core_pattern` handling;
- saved-core analysis in a fresh GDB process with required `SIGSEGV`, `triggerNullDereference`, and `main` backtrace markers;
- a historical heap use-after-free reproduction isolated to the lab tool and Git history;
- a repaired lifetime path that keeps access inside the `std::unique_ptr` ownership window and clears the non-owning pointer after owner reset;
- ASan+UBSan verification that the repaired path exits successfully without sanitizer diagnostics;
- safe CTest regression for the crash-generator CLI and repaired lifetime path;
- a consolidated `scripts/phase9/verify.sh` entry point that inherits the Phase 8 gate;
- CI sanitizer coverage for the safe Phase 9 regression.

Deliberately excluded from this phase:

- introducing intentional undefined behavior into the production softphone path;
- treating a lab-only null dereference as a production defect that should be "fixed";
- committing core files, raw GDB transcripts, or sanitizer crash logs as permanent evidence;
- changing SIP, RTP, Qt, worker, or callback ownership solely to manufacture a debugging example;
- claiming that one clean sanitizer run proves the absence of all memory defects;
- changing the Phase 8 worker/threading architecture.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| Controlled crash tool | Provide deterministic lab-only failure and lifetime scenarios | `tools/crash_generator/main.cpp`, `tools/crash_generator/CMakeLists.txt` |
| Saved-core workflow | Capture `SIGSEGV`, generate a core, and reopen it in GDB | `scripts/phase9/capture-core-dump.sh` |
| Memory diagnostics | Run the repaired historical UAF path under ASan+UBSan | `scripts/phase9/verify-asan-use-after-free.sh` |
| Safe regression | Keep the CLI contract and repaired lifetime path in CTest | `CMakeLists.txt` |
| Aggregate verifier | Reuse Phase 8 checks and add Phase 9 GDB/sanitizer gates | `scripts/phase9/verify.sh` |
| CI coverage | Exercise sanitizer and ThreadSanitizer CTest jobs with the safe regression | `.github/workflows/test.yml` |
| Architecture note | Explain fault isolation, core analysis, lifetime repair, and verification boundaries | `docs/architecture/debugging-diagnostics.md` |
| Core analysis note | Record the reproducible crash/lifetime investigation workflow | `docs/phase9/core-dump-analysis.md` |
| Interview review | Review GDB, cores, ASan, ownership, and limitations | `docs/interview/phase9-questions.md` |

## Architecture and design

Phase 9 adds a debugging laboratory beside the production application rather than inside it:

```text
production softphone
    -> unchanged Phase 8 application/threading architecture

lab-only diagnostics
    -> crash_generator
       ├── null-deref
       │    -> GDB stops on SIGSEGV
       │    -> generate-core-file
       │    -> fresh GDB opens executable + saved core
       │    -> validated backtrace
       │
       └── use-after-free
            -> repaired lifetime path
            -> normal CTest regression
            -> ASan+UBSan clean verification
```

The `null-deref` path remains intentionally crashing because its purpose is to make the core-dump workflow deterministic. It is not registered as an ordinary CTest case. The safe `--help` and repaired `use-after-free` paths are the cases that run in normal and sanitizer-oriented CTest jobs.

The memory-lifetime lesson is intentionally narrow. A `std::unique_ptr<int>` owns the heap object, while a temporary raw pointer observes it only while the owner is alive. The repaired path reads the value before `owner.reset()`, then clears the observer and performs no post-destruction access. The historical faulty revision did the opposite: it retained a raw alias, destroyed the owner, and dereferenced the dangling pointer.

See `docs/architecture/debugging-diagnostics.md` for the detailed model and `docs/phase9/core-dump-analysis.md` for the investigation report.

## Build and run

The complete Phase 9 verifier inherits the Phase 8 Qt/PJSIP verification path, so expose the local PJPROJECT 2.17 pkg-config directory first:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase9/verify.sh
```

Run the saved-core workflow directly with:

```bash
./scripts/phase9/capture-core-dump.sh
```

By default it writes disposable artifacts under:

```text
build/phase9-core/
├── crash_generator.core
├── gdb-live.txt
└── gdb-core-backtrace.txt
```

Run the repaired lifetime path under ASan+UBSan with:

```bash
./scripts/phase9/verify-asan-use-after-free.sh
```

Its disposable log is written under `build/phase9-asan/`.

The safe crash-generator regression can also be inspected directly:

```bash
ctest --test-dir build/pjsip-debug -L phase9 --output-on-failure
```

## Verification

The Phase 9 automated gate covers:

- repository diff checks;
- the complete inherited Phase 8 verification gate;
- at least two Phase 9-labeled safe CTest cases;
- deterministic GDB stop on the intentional `SIGSEGV`;
- explicit `generate-core-file` output and fresh saved-core analysis;
- required saved-core backtrace markers;
- a fresh ASan+UBSan build for the repaired lifetime path;
- successful repaired-path exit with no ASan/UBSan error marker;
- Phase 9 CTest regression in the sanitizer build;
- CI execution of the safe Phase 9 regression in sanitizer-oriented jobs.

The latest pre-documentation PJSIP debug rerun reported 141/141 tests passing, including both Phase 9 tests. `Phase9RepairedLifetimeRegressionTest` was also repeated 20 times with no failure. Those counts are observations from that build, not permanent contract values; the verifier checks the Phase 9 label and minimum required test count rather than hard-coding the full PJSIP suite size.

The intentional null dereference is verified through GDB instead of CTest because a normal CTest success criterion should not require a process crash. Conversely, the repaired historical UAF path is intentionally safe and belongs in ordinary regression suites.

## Evidence

The accepted Phase 9 evidence set is:

```text
evidence/phase9/
├── README.md
├── automated-verification.txt
├── gdb-core-analysis.txt
└── asan-report.txt
```

The retained files are concise, sanitized summaries. They record the aggregate Phase 9 verifier, the saved-core GDB contract, the historical heap-use-after-free lesson, the repaired ASan/UBSan-clean result, and the branch CI outcome without committing bulky or host-specific binary artifacts.

Do not commit:

- `*.core` files;
- raw GDB session dumps when a concise reviewed summary is sufficient;
- raw sanitizer crash transcripts containing unnecessary environment/path detail;
- SIP credentials, Digest authorization material, media keying material, or packet captures inherited from other phases.

GDB/core/sanitizer working files remain disposable under `build/`; only the reviewed summaries above belong in committed evidence.

## Interview focus

Use `docs/interview/phase9-questions.md` together with the scripts and crash generator. Be ready to explain:

- why controlled faults live in a standalone lab tool rather than the production softphone;
- why the workflow uses GDB `generate-core-file` instead of assuming Linux `core_pattern` behavior;
- why a saved core must be analyzed with the matching executable/debug information;
- what a backtrace can reveal and what it cannot prove by itself;
- how the historical raw pointer became dangling after `std::unique_ptr::reset()`;
- why using `std::unique_ptr` does not automatically make every raw alias safe;
- what ASan detects dynamically and what a clean run does not guarantee;
- why the repaired scenario keeps the old `use-after-free` CLI name as a regression contract;
- why the intentionally crashing path is excluded from ordinary CTest while the repaired path is included;
- why the sanitizer build tree is recreated to avoid stale-object false results;
- how Phase 9 preserves the Phase 8 threading and GUI boundaries.

## Exit criteria

Phase 9 closure requires:

- the controlled crash generator to remain isolated from production runtime paths;
- the `null-deref` scenario to reproducibly stop with `SIGSEGV` under GDB;
- GDB to write a non-empty saved core and a fresh GDB process to recover the required backtrace markers;
- the historical UAF to have an explicit root-cause explanation tied to object lifetime;
- the repaired lifetime path to perform no access after owner destruction;
- ASan+UBSan verification to complete without memory/undefined-behavior diagnostics on the repaired path;
- the safe Phase 9 CTest cases to pass in normal and sanitizer-oriented configurations;
- the complete Phase 9 verifier to pass on the documented environment;
- architecture, core-analysis, interview-review, and sanitized Phase 9 evidence to be present;
- no intentional UB, core file, or raw diagnostic dump to leak into the production/evidence boundary.

The recorded local verification, repeated repaired-lifetime regression, saved-core analysis, sanitizer verification, CI 4/4 result, documentation, and curated evidence satisfy these criteria for the tested branch state. The `v0.9-diagnostics` tag is created only after review, merge to `main`, and verification of the merged closure state.

## References

- Architecture: `docs/architecture/debugging-diagnostics.md`
- Core/lifetime analysis: `docs/phase9/core-dump-analysis.md`
- Prior threading architecture: `docs/architecture/threading-model.md`
- Interview review: `docs/interview/phase9-questions.md`
- Aggregate verifier: `scripts/phase9/verify.sh`
- Core workflow: `scripts/phase9/capture-core-dump.sh`
- ASan lifetime verifier: `scripts/phase9/verify-asan-use-after-free.sh`
- Crash generator: `tools/crash_generator/main.cpp`
- CTest registration: `CMakeLists.txt`

## Next phase

Phase 10 is the RTSP mini-lab from the roadmap. It should remain a separate protocol exercise rather than forcing RTSP into the SIP softphone architecture.
