# Phase 2 — Linux System Programming

## Status

**Completed** — milestone tag `v0.3-linux-system`.

Phase 2 applies the Modern C++ ownership model to Linux process, signal, file-descriptor, and local IPC primitives.

## Objective

Build a reusable Linux systems foundation before introducing Internet sockets and VoIP protocols. The phase demonstrates direct POSIX APIs while keeping resource lifetime explicit through C++ RAII.

## Scope

Included in this phase:

- move-only file-descriptor ownership;
- `fork()` process creation;
- `execv()` process-image replacement;
- `waitpid()` parent/child lifecycle handling;
- `sigaction()` signal installation;
- signal-safe `volatile sig_atomic_t` state;
- `sigprocmask()` and `sigsuspend()` for race-aware waiting;
- Unix Domain Socket server/client abstractions;
- real IPC integration tests.

Deliberately excluded:

- IPv4 TCP/UDP networking;
- SIP/RTP protocols;
- background worker-thread architecture.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| File-descriptor RAII | Own one POSIX descriptor and close exactly once | `src/platform/linux/UniqueFd.*` |
| Process lifecycle lab | Demonstrate `fork()` and `waitpid()` | `labs/linux/process-lifecycle/` |
| Exec lab | Demonstrate process-image replacement | `labs/linux/exec-child/` |
| Signal lab | Demonstrate safe signal setup and waiting | `labs/linux/signal-handling/` |
| IPC server | Local `AF_UNIX` / `SOCK_STREAM` listener | `src/ipc/UnixDomainSocketServer.*` |
| IPC client | Matching local client abstraction | `src/ipc/UnixDomainSocketClient.*` |
| Integration tests | Exercise real client/server round trips | `tests/integration/test_unix_domain_socket_ipc.cpp` |
| Architecture note | Explain process, signal, FD, and IPC design | `docs/architecture/linux-process-ipc.md` |
| Interview review | Capture Phase 2 review questions | `docs/interview/phase2-questions.md` |

## Architecture and design

The reusable ownership model is:

```text
POSIX descriptor
      |
      v
UniqueFd
      |
      +--> UnixDomainSocketServer
      +--> UnixDomainSocketClient
```

Important design decisions:

- `UniqueFd` is move-only, preventing accidental duplicate ownership;
- `release()` transfers the raw descriptor without closing it;
- `reset()` closes the current descriptor before adopting another;
- `waitpid()` handles interruption with `EINTR` retries;
- signal handlers perform minimal signal-safe work and defer normal C++ logic;
- `sigsuspend()` avoids a check-then-sleep race;
- the Unix Domain Socket server removes stale socket paths but refuses to overwrite regular files;
- integration tests use the actual kernel IPC path instead of mocked transports.

## Build and run

Build and run the normal debug tests:

```bash
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug --output-on-failure
```

The phase also provides focused Linux labs for process lifecycle, exec replacement, and signal handling.

## Verification

The phase verifier exercises:

- unit tests for `UniqueFd` and IPC state;
- process and signal labs;
- Unix Domain Socket integration tests;
- formatting and static analysis;
- ASan and UBSan;
- full regression tests;
- `git diff --check`.

Run:

```bash
./scripts/verify_phase2.sh
```

## Evidence

Committed evidence lives under:

```text
evidence/02-linux/
```

The evidence records build/test/analysis results together with process, exec, signal, and IPC demonstrations.

## Interview focus

Use `docs/interview/phase2-questions.md` together with the implementation. Be ready to explain:

- file descriptors and RAII;
- `fork()` return values and process relationships;
- `fork()` versus `exec()`;
- why parents reap children with `waitpid()`;
- `EINTR` handling;
- `sigaction()` and async-signal-safety;
- `sigsuspend()`;
- Unix Domain Sockets versus TCP for local IPC;
- `bind()`, `listen()`, `accept()`, and socket-path cleanup.

## Exit criteria

Phase 2 is complete when all of the following statements are true:

- file-descriptor ownership is move-only and deterministic;
- process lifecycle and exec replacement are demonstrated;
- signal handling avoids unsafe work in handlers;
- Unix Domain Socket client/server communication passes integration tests;
- formatting, static analysis, sanitizers, and full tests pass;
- architecture and interview documentation are present;
- Phase 2 evidence is committed;
- milestone tag `v0.3-linux-system` identifies the completed phase.

## References

- Architecture: `docs/architecture/linux-process-ipc.md`
- Interview review: `docs/interview/phase2-questions.md`
- Evidence: `evidence/02-linux/`

## Next phase

Phase 3 reuses `UniqueFd` and the integration-test approach to build IPv4 TCP and UDP socket abstractions.
