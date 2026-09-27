# Phase 3 — TCP/UDP Networking Foundation

## Goal

Phase 3 builds and verifies the Linux IPv4 socket foundation required by later VoIP protocol work.
It deliberately stops below SIP and RTP: this phase is about endpoint representation, descriptor
ownership, TCP lifecycle, UDP datagrams, error handling, and repeatable loopback tests.

## Implemented components

```text
Ipv4Endpoint
TcpServerSocket
TcpClientSocket
UdpSocket
```

The implementation uses Linux/POSIX networking calls including:

```text
socket
setsockopt
bind
listen
accept
connect
getsockname
sendto
recvfrom
inet_pton
inet_ntop
htons
ntohs
```

The TCP integration tests additionally exercise `send`, `recv`, and `getpeername` through native
file descriptors.

## Design decisions

### One endpoint value type

`Ipv4Endpoint` centralizes parsing, formatting, `sockaddr_in` conversion, and port byte order. Socket
classes therefore operate on the same C++ endpoint type rather than duplicating address conversion.

### RAII for every owned descriptor

Socket wrappers reuse `platform::UniqueFd`. A descriptor either belongs to one RAII object or is
invalid. Setup is performed with temporary `UniqueFd` instances so failures close descriptors
automatically.

### Ephemeral ports for tests

Tests bind `127.0.0.1:0`. Linux chooses an available port, and `getsockname()` reports the actual
endpoint. This avoids hard-coded test ports and makes repeated local runs more reliable.

### Blocking I/O first

The Phase 3 layer is intentionally synchronous and blocking. This isolates basic socket semantics
before the later multithreading/event-loop work introduces concurrency and cancellation concerns.

### Binary-safe UDP payloads

UDP uses `std::span<std::byte>` rather than strings. The integration tests include zero and
high-bit bytes to verify that payload handling is not text-specific.

## Verification coverage

### IPv4 endpoint

```text
stores address and port
converts to sockaddr_in using network byte order
supports wildcard address
rejects empty/malformed addresses
rejects non-IPv4 sockaddr values
```

### TCP server

```text
starts stopped
binds loopback with an ephemeral port
start is idempotent
stop closes listener and clears endpoint
accept requires a listening socket
```

### TCP client

```text
starts disconnected
connects to a listening server
connect is idempotent
disconnect closes socket and clears endpoint
can reconnect after disconnect
```

### TCP integration

```text
request/response payload round-trip
matching client/server local and peer endpoints
sequential client connections on one listener
```

### UDP socket

```text
starts unbound
binds loopback with an ephemeral port
bind is idempotent
close clears socket state
send requires a bound socket
receive requires a bound socket
```

### UDP integration

```text
bidirectional datagram round-trip
binary payload preservation
sender endpoint reporting
sequential datagram receive
```

At the end of Commit 6, the repository test suite contains 60 tests on the development branch. The
phase evidence commit should record the final verification results rather than relying only on this
documented checkpoint.

## Quality gates

Run the same gates before the Phase 3 evidence commit:

```bash
find src tests labs -type f \
    \( -name '*.cpp' -o -name '*.hpp' \) \
    -print0 | xargs -0 clang-format --dry-run --Werror

cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug --output-on-failure

./scripts/run_sanitizers.sh

rm -rf build/clang-tidy
./scripts/run_clang_tidy.sh

git diff --check
```

## Relationship to later VoIP phases

The networking layer is intentionally protocol-neutral.

```text
Phase 3 socket primitives
        |
        +--> later SIP/PJSIP signaling work
        |
        +--> later RTP/media work
        |
        +--> later threading/event-loop integration
```

`UdpSocket` already accepts binary buffers and returns the sender endpoint, which maps naturally to
future RTP transport experiments. TCP support provides a tested stream transport and reinforces
stream-framing concepts useful when working with signaling or control protocols.

No SIP message grammar, RTP header, codec, jitter buffer, or audio-device behavior belongs to
Phase 3.

## Review checklist

Before considering the networking phase complete, be able to explain without reading a definition:

```text
host byte order vs network byte order
why port 0 is useful in tests
why TCP send/recv need loops
why TCP does not preserve application message boundaries
why UDP receive returns a sender endpoint
what EINTR means for the current blocking calls
how UniqueFd prevents descriptor leaks
why getsockname is used after binding/connecting
why loopback integration tests are valuable
what Phase 3 intentionally defers to later phases
```

See `docs/architecture/networking.md` for the detailed design and
`docs/interview/phase3-networking-questions.md` for interview-oriented review prompts.
