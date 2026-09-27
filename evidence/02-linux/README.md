# Phase 2 evidence — Linux system programming and IPC

Collected at: 2026-09-27T18:04:09+07:00

Branch: `phase/2-linux-system`  
HEAD at collection time: `684144b`

This directory is the reproducible evidence snapshot for Phase 2. The collector
exits immediately if a build, test, sanitizer, clang-tidy, or clang-format gate
fails.

## Evidence files

- `toolchain.txt` — compiler, CMake, Git, Clang tooling, collection timestamp.
- `build.log` — Debug configure/build output.
- `unit-tests.log` — GoogleTest unit suite, including UniqueFd and Unix socket unit tests.
- `integration-tests.log` — Unix Domain Socket client/server end-to-end tests.
- `all-tests.log` — complete CTest regression suite.
- `process-lifecycle-demo.log` — fork/waitpid process lifecycle self-test.
- `exec-child-demo.log` — fork/exec process-image replacement self-test.
- `signal-handling-demo.log` — sigaction/sigsuspend graceful signal self-test.
- `sanitizers.log` — clean ASan + UBSan rebuild and test run.
- `clang-tidy.log` — clean clang-tidy rebuild.
- `format-check.log` — clang-format gate.
- `git-history.txt` — commit history visible at collection time.
- `git-status.txt` — working-tree state at collection time.

## Phase 2 capabilities represented

- RAII ownership for Linux file descriptors.
- `fork()`, `execv()`, `waitpid()`, PID/process lifecycle.
- POSIX signal handling with `sigaction()` and race-aware waiting via `sigsuspend()`.
- Unix Domain Socket server/client lifecycle using `AF_UNIX` and `SOCK_STREAM`.
- Client/server IPC round-trip and sequential-connection integration tests.

The final evidence commit itself is expected not to appear in
`git-history.txt`, because this snapshot must be generated before that commit
exists.
