# Phase 2 Interview Review — Linux System Programming

Use these questions to explain the engineering decisions in Phase 2 rather than memorizing isolated syscall definitions.

## File descriptors and RAII

### Why wrap a file descriptor in a C++ class?

A raw descriptor is an integer with manual lifetime management. An RAII wrapper ties `close()` to object lifetime, makes ownership visible in the type system, and prevents many leak/double-close paths.

### Why is `UniqueFd` move-only?

Two independent owners closing the same descriptor would violate ownership semantics. Move operations transfer the descriptor and invalidate the source object; copying is disabled.

### What is the difference between `release()` and `reset()`?

`release()` returns the descriptor without closing it and leaves the wrapper empty. `reset()` closes the currently owned descriptor, if any, and optionally adopts a replacement descriptor.

## Processes

### What does `fork()` return?

The parent receives the child's PID, the child receives `0`, and failure is reported with `-1`.

### Why must a parent call `waitpid()`?

It collects the child's termination status and reaps the child so it does not remain as a zombie process.

### What is the difference between `fork()` and `exec()`?

`fork()` creates a new process. `exec()` replaces the current process image. A successful `exec()` does not return and preserves the process PID.

### Why handle `EINTR` around blocking system calls?

A signal can interrupt a blocking syscall before it completes. For operations where retrying is correct, the caller should detect `EINTR` and retry instead of treating it as a permanent failure.

## Signals

### Why use `sigaction()` instead of the older `signal()` API?

`sigaction()` provides explicit handler configuration, mask control, and well-defined flags, making behavior easier to reason about for production POSIX code.

### Why should a signal handler do very little work?

Only async-signal-safe operations are valid inside a signal handler. The phase therefore records state in `volatile sig_atomic_t` and performs normal C++ shutdown work after returning to regular execution context.

### Why use `sigsuspend()`?

It atomically replaces the signal mask while waiting. That avoids the race window in a pattern that checks a flag and then separately calls `pause()`.

## Unix Domain Sockets

### Why use `AF_UNIX` here instead of TCP?

The purpose of this phase is local IPC between processes on the same Linux host. Unix Domain Sockets provide stream semantics without IP routing and expose a filesystem path as the local endpoint.

### What is the role of `bind()`, `listen()`, and `accept()` on the server?

`bind()` associates the socket with its Unix path, `listen()` marks it as a passive listening socket, and `accept()` creates a connected descriptor for one client connection.

### Why does the server unlink the socket path?

Closing an `AF_UNIX` socket descriptor does not automatically remove its filesystem path. The server owns cleanup of that path during controlled shutdown/destruction.

### Why should the server refuse to overwrite a regular file?

Blindly unlinking whatever exists at the configured path could destroy unrelated user data. The implementation distinguishes a stale socket endpoint from an ordinary file.

## Testing

### Why have both unit tests and IPC integration tests?

Unit tests isolate ownership and lifecycle behavior. Integration tests prove that the real client and server interoperate through actual Unix Domain Sockets and the kernel, including connection acceptance and data round trips.

### What does the `PING`/`PONG` test prove?

It verifies a complete bidirectional stream path: client connection, server acceptance, client-to-server data transfer, server-to-client response, and cleanup.

## Design discussion

### How does Phase 2 prepare for TCP/UDP networking?

The project now has an established descriptor-ownership model, syscall error-handling conventions, integration-test structure, and experience with blocking I/O. Those patterns can be reused for IP sockets.

### What would you improve for production-grade IPC?

Possible extensions include non-blocking I/O, `poll`/`epoll`, framed messages, timeouts, authentication/permissions, multiple concurrent clients, backpressure, structured error types, and explicit shutdown coordination.
