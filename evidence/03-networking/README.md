# Phase 3 Evidence — TCP/UDP Networking

This directory stores verification evidence generated on the developer's Ubuntu machine for Phase 3.
The logs must come from real local executions; they are intentionally not pre-generated in this ZIP.

Run the complete Phase 3 gate first:

```bash
./scripts/verify_phase3.sh
```

Then collect the evidence:

```bash
./scripts/collect_phase3_evidence.sh
```

The collector writes:

```text
toolchain.txt
build.log
network-tests.log
full-tests.log
sanitizers.log
clang-tidy.log
format-check.log
git-diff-check.log
git-history.txt
phase3-summary.txt
```

## What this evidence proves

- `build.log` records a successful Debug configuration and build.
- `network-tests.log` isolates the Phase 3 IPv4/TCP/UDP unit and integration tests.
- `full-tests.log` records the complete project regression suite.
- `sanitizers.log` records the ASan/UBSan build and test run.
- `clang-tidy.log` records static-analysis verification with warnings treated as errors by project policy.
- `format-check.log` records the repository C++ formatting gate.
- `git-diff-check.log` records Git whitespace validation.
- `git-history.txt` ties the evidence to the Phase 3 commit sequence.
- `toolchain.txt` records the local compiler/build-analysis toolchain and the exact branch/commit tested.

At the current Phase 3 baseline, the full regression suite is expected to contain 60 tests before this
evidence-only commit. If that number changes intentionally, update this README and `phase3-summary.txt`
so the evidence remains self-consistent.

Do not hand-edit passing output into these files. Re-run the collector whenever the tested commit changes.
