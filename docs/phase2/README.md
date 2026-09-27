# Phase 2 — Linux System Programming

Phase 2 builds the Linux systems foundation used by the VoIP softphone before higher-level SIP, RTP, and UI work begins.

## Goals

This phase demonstrates practical Linux programming with C++20:

- deterministic file-descriptor ownership with RAII,
- process creation with `fork()`,
- process-image replacement with `execv()`,
- parent/child lifecycle management with `waitpid()`,
- signal handling with `sigaction()`,
- signal-safe state using `volatile sig_atomic_t`,
- race-aware waiting with `sigprocmask()` and `sigsuspend()`,
- local IPC with Unix Domain Sockets,
- end-to-end client/server integration tests.

## Implemented milestones

### 1. RAII file-descriptor ownership

`voip::platform::UniqueFd` owns one POSIX file descriptor and closes it automatically on destruction.

Important semantics:

- copy construction and copy assignment are disabled,
- move construction and move assignment transfer ownership,
- `release()` gives ownership back to the caller without closing,
- `reset()` closes the currently owned descriptor before adopting a new one.

This type is reused by the Unix Domain Socket implementation so resource lifetime is explicit and exception-safe.

### 2. Process lifecycle lab

The process lifecycle lab demonstrates:

1. a parent process calling `fork()`,
2. the child observing its PID and parent PID,
3. the child exiting with a known status,
4. the parent using `waitpid()` to reap the child,
5. `WIFEXITED()` and `WEXITSTATUS()` to interpret termination.

The `waitpid()` path retries when interrupted by `EINTR`.

### 3. `exec()` process replacement lab

The exec lab demonstrates the distinction between creating a process and replacing a process image.

The child created by `fork()` calls `execv()` on `/proc/self/exe`. The replacement process verifies that its PID is preserved across `exec()` and exits with a known code for the parent to collect.

Key idea:

> `fork()` creates a new process. `exec()` replaces the current process image; it does not create another PID.

### 4. POSIX signal handling

The signal-handling lab installs handlers with `sigaction()` for `SIGINT` and `SIGTERM`.

The handler only updates `volatile sig_atomic_t` state. Normal C++ work is performed outside the signal handler.

The lab blocks the relevant signals during setup and uses `sigsuspend()` for race-aware waiting rather than a check-then-`pause()` loop.

### 5. Unix Domain Socket server

`voip::ipc::UnixDomainSocketServer` provides a local `AF_UNIX` / `SOCK_STREAM` server abstraction.

Responsibilities include:

- validating the socket path,
- creating the listening socket,
- handling stale socket filesystem entries,
- refusing to overwrite a regular file,
- `bind()`, `listen()`, and `accept()`,
- retrying interrupted `accept()` calls,
- removing the socket path during shutdown/destruction.

### 6. Unix Domain Socket client

`voip::ipc::UnixDomainSocketClient` provides the matching local client abstraction.

Responsibilities include:

- validating the target path,
- creating an `AF_UNIX` stream socket,
- connecting to the server,
- reporting connection failures,
- disconnecting through RAII-managed descriptor ownership.

### 7. IPC integration tests

The integration tests exercise the real server and client together rather than mocking either side.

Covered scenarios include:

- request/response round trip (`PING` → `PONG`),
- server-side `accept()` with a real client,
- sequential client connections,
- cleanup of the Unix socket path.

## Engineering practices used in this phase

Phase 2 keeps the same quality gates established earlier in the project:

- C++20,
- CMake + Ninja,
- GoogleTest,
- `clang-format`,
- `clang-tidy`,
- AddressSanitizer,
- UndefinedBehaviorSanitizer,
- GitHub Actions.

The implementation is deliberately split into atomic commits so each Linux concept can be reviewed independently.

## Relationship to later phases

The primitives introduced here are intentionally reusable:

- `UniqueFd` can own network sockets in the TCP/UDP phase,
- process and signal knowledge supports service-style components and diagnostics,
- Unix Domain Sockets provide a concrete IPC mechanism for separating future UI, diagnostics, or worker processes,
- the integration-test structure becomes a template for later network and SIP tests.

## Suggested demo sequence

A short Phase 2 demonstration can be presented in this order:

1. show `UniqueFd` ownership tests,
2. run the `fork()` lifecycle lab,
3. run the `exec()` replacement lab,
4. run the signal self-test,
5. run the Unix Domain Socket integration tests,
6. run sanitizer and static-analysis gates.

This gives a reviewer a progression from low-level resource ownership to complete local IPC.
