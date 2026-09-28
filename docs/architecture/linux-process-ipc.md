# Phase 2 Architecture — Linux Process and IPC

## Status

Completed — milestone `v0.3-linux-system`.

## Purpose

Phase 2 establishes the Linux resource-ownership, process, signal, and Unix Domain Socket conventions used by later system components.

## Scope

- Own POSIX file descriptors with a move-only RAII type.
- Separate `fork()`, `exec()`, and `waitpid()` responsibilities.
- Keep signal handlers minimal and signal-safe.
- Provide lifecycle-managed Unix Domain Socket client/server primitives.

## Architecture model

### Overview

Phase 2 introduces the Linux primitives that future VoIP components can build on. The design keeps operating-system resource ownership explicit and separates process, signal, and IPC concerns.

```text
+-------------------------+
| Application / Tests     |
+------------+------------+
             |
             v
+-------------------------+
| IPC abstractions        |
| - Unix socket server    |
| - Unix socket client    |
+------------+------------+
             |
             v
+-------------------------+
| Linux resource layer    |
| - UniqueFd              |
+------------+------------+
             |
             v
+-------------------------+
| POSIX / Linux syscalls  |
| socket, bind, listen    |
| accept, connect, close  |
| fork, execv, waitpid    |
| sigaction, sigsuspend   |
+-------------------------+
```

### File-descriptor ownership

Raw POSIX APIs return integer file descriptors. The project wraps descriptor ownership in `voip::platform::UniqueFd` so lifetime is represented by an object rather than by convention.

The invariant is:

> A valid descriptor is owned by exactly one `UniqueFd` instance unless ownership has explicitly been released.

This reduces duplicated `close()` paths and makes early-return and exception paths safer.

### Process model

The process labs distinguish three responsibilities:

- `fork()` creates the child process,
- `execv()` replaces a process image while preserving the PID,
- `waitpid()` gives the parent an explicit reaping and exit-status boundary.

The project treats `EINTR` as a recoverable interruption for blocking system calls that can be retried safely.

### Signal model

Signal handlers are intentionally minimal. They update signal-safe state only; normal C++ logic remains in regular execution context.

The signal lab uses a block/setup/wait pattern:

```text
block signal
    ↓
install sigaction handler
    ↓
prepare normal program state
    ↓
sigsuspend() with temporary mask
    ↓
handler updates sig_atomic_t state
    ↓
normal code performs shutdown work
```

This avoids doing non-async-signal-safe C++ operations inside the handler.

### Unix Domain Socket model

The IPC transport is a stream-oriented Unix Domain Socket:

```text
Server                                    Client
------                                    ------
socket(AF_UNIX, SOCK_STREAM)              socket(AF_UNIX, SOCK_STREAM)
        |                                         |
        v                                         v
bind(path)                                   connect(path)
        |
        v
listen()
        |
        v
accept() <-------------------------------- connected socket
        |
        +------------- bidirectional stream -----+
```

The socket path is a filesystem-visible endpoint. Server startup therefore owns both a file descriptor and a filesystem cleanup responsibility.

## Runtime and lifecycle

### Server lifecycle

```text
constructed
   |
   v
start()
   |
   +-- validate path
   +-- inspect stale path
   +-- socket()
   +-- bind()
   +-- listen()
   |
   v
listening
   |
   +-- accept_one()
   |
   v
stop()/destructor
   |
   +-- close descriptor through UniqueFd
   +-- unlink socket path
```

The server refuses to replace an ordinary file at the configured socket path.

### Client lifecycle

```text
constructed
   |
   v
connect_to(path)
   |
   +-- validate path
   +-- socket()
   +-- connect()
   |
   v
connected
   |
   v
disconnect()/destructor
   |
   v
descriptor closed through UniqueFd
```

## Verification strategy

### Test strategy

The phase uses two levels of verification.

Unit tests validate ownership and lifecycle rules independently:

- `UniqueFd` move/release/reset behavior,
- server path creation and cleanup,
- invalid path handling,
- client connection and disconnection behavior.

Integration tests then run the real client and server together and verify data movement and repeated connections.

This separation makes failures easier to localize while still proving that the components interoperate through the real kernel IPC path.

## Design rationale

### Why this matters for the VoIP project

Later phases introduce TCP/UDP sockets, SIP signaling, RTP media, worker threads, and diagnostics. Phase 2 establishes the low-level resource-management and IPC conventions those components can follow instead of introducing ad-hoc descriptor and process handling later.

## Phase boundary

Phase 2 focuses on local Linux primitives and IPC. It intentionally stops before IPv4 TCP/UDP networking, SIP signaling, RTP media, or GUI work.

## References

- `docs/phase2/README.md`
- `docs/interview/phase2-questions.md`
- `docs/architecture/linux-process-ipc.md`
- `src/platform/linux/UniqueFd.hpp`
- `src/ipc/UnixDomainSocketServer.hpp`
- `tests/integration/test_unix_domain_socket_ipc.cpp`
