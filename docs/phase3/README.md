# Phase 3 — TCP/UDP Networking Foundation

## Status

**Completed** — milestone tag `v0.4-networking`.

Phase 3 establishes protocol-neutral IPv4 socket primitives and real loopback integration tests before SIP and RTP are introduced.

## Objective

Implement and verify TCP/UDP behavior directly on Linux sockets so later VoIP work is built on understood transport semantics rather than hidden behind a SIP library.

## Scope

Included in this phase:

- IPv4 endpoint parsing and `sockaddr_in` conversion;
- host/network byte-order handling;
- TCP server lifecycle;
- TCP client lifecycle;
- UDP bind/send/receive lifecycle;
- binary-safe datagrams;
- real loopback TCP/UDP integration tests;
- explicit `EINTR` handling where required.

Deliberately excluded:

- DNS and IPv6;
- TLS;
- non-blocking I/O, `epoll`, or event loops;
- reusable TCP message framing;
- SIP transactions or RTP headers/media.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| IPv4 endpoint | Centralize address/port conversion | `src/network/Ipv4Endpoint.*` |
| TCP server | Own bind/listen/accept lifecycle | `src/network/TcpServerSocket.*` |
| TCP client | Own connect/disconnect lifecycle | `src/network/TcpClientSocket.*` |
| UDP socket | Own bind/send/receive lifecycle | `src/network/UdpSocket.*` |
| Unit tests | Verify state and validation behavior | `tests/unit/test_*socket.cpp`, `test_ipv4_endpoint.cpp` |
| Integration tests | Exercise real kernel TCP/UDP paths | `tests/integration/test_tcp_network.cpp`, `test_udp_network.cpp` |
| Architecture note | Explain networking design and boundaries | `docs/architecture/networking.md` |
| Interview review | Capture Phase 3 review questions | `docs/interview/phase3-questions.md` |

## Architecture and design

The Phase 3 networking layer is intentionally synchronous and small:

```text
Ipv4Endpoint
     |
     +--> TcpServerSocket --> accepted UniqueFd
     +--> TcpClientSocket
     +--> UdpSocket

all owned descriptors --> platform::UniqueFd
```

Key decisions:

- one endpoint type owns address parsing/formatting and byte-order conversion;
- every owned descriptor is managed through RAII;
- tests bind to port `0` so Linux chooses an available ephemeral port;
- `getsockname()` discovers the actual local endpoint;
- TCP tests loop around `send()`/`recv()` because TCP is a byte stream;
- UDP uses `std::span<std::byte>` so payloads are binary-safe;
- blocking I/O is deliberate so transport fundamentals remain isolated from later concurrency work.

## Build and run

Build and run the debug suite:

```bash
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug --output-on-failure
```

Focused network tests can be selected through CTest labels/names in the generated build tree.

## Verification

Phase 3 verification covers:

- IPv4 endpoint validation and byte-order conversion;
- TCP bind/listen/accept/connect/disconnect behavior;
- TCP payload round trips and endpoint relationships;
- UDP bind/send/receive behavior;
- binary datagram preservation and sender reporting;
- sequential client/datagram scenarios;
- formatting, full tests, sanitizers, static analysis, and `git diff --check`.

Run:

```bash
./scripts/verify_phase3.sh
```

## Evidence

Committed evidence lives under:

```text
evidence/03-networking/
```

The evidence records network-focused tests plus the full project quality gates used to close the phase.

## Interview focus

Use `docs/interview/phase3-questions.md` together with the implementation. Be ready to explain:

- TCP versus UDP semantics;
- host versus network byte order;
- `htons()`, `ntohs()`, `inet_pton()`, and `inet_ntop()`;
- why tests use ephemeral ports;
- why TCP does not preserve application message boundaries;
- what `accept()` returns and who owns the accepted descriptor;
- why UDP receive reports the sender endpoint;
- `SO_REUSEADDR` and `EINTR` in the implemented scope;
- why loopback integration tests are useful but not equivalent to real-network testing.

## Exit criteria

Phase 3 is complete when all of the following statements are true:

- endpoint parsing and byte-order conversion are covered by tests;
- TCP client/server round trips pass using real kernel sockets;
- UDP bidirectional and binary-payload tests pass;
- descriptor ownership remains leak-safe through `UniqueFd`;
- formatting, static analysis, sanitizers, and regressions pass;
- architecture and interview documentation are present;
- Phase 3 evidence is committed;
- milestone tag `v0.4-networking` identifies the completed phase.

## References

- Architecture: `docs/architecture/networking.md`
- Interview review: `docs/interview/phase3-questions.md`
- Evidence: `evidence/03-networking/`

## Next phase

Phase 4 introduces SIP and Asterisk on a real LAN lab while keeping the custom C++ client out of scope until the signaling environment is proven.
