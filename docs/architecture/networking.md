# Phase 3 Architecture — TCP/UDP Networking

## Status

Completed — milestone `v0.4-networking`.

## Purpose

Phase 3 provides a small Linux IPv4 networking layer that can be exercised independently from
SIP, RTP, PJSIP, and Qt. The goal is to make socket ownership, endpoint conversion, TCP connection
lifecycle, and UDP datagram semantics explicit before higher-level VoIP protocols are introduced.

The production components are:

```text
src/network/
├── Ipv4Endpoint.cpp
├── Ipv4Endpoint.hpp
├── TcpClientSocket.cpp
├── TcpClientSocket.hpp
├── TcpServerSocket.cpp
├── TcpServerSocket.hpp
├── UdpSocket.cpp
└── UdpSocket.hpp
```

All socket file descriptors are owned through `voip::platform::UniqueFd`, so descriptor lifetime is
controlled by RAII instead of manual cleanup at every return path.

## Scope

- Represent IPv4 address/port values explicitly at the C++ boundary.
- Own TCP and UDP sockets with RAII and clear lifecycle transitions.
- Exercise real loopback TCP stream and UDP datagram behavior.
- Keep Phase 3 independent from SIP, RTP, PJSIP, and Qt.

## Architecture model

### Component model

```text
                         +------------------+
                         |  Ipv4Endpoint    |
                         | address + port   |
                         +---------+--------+
                                   |
                 +-----------------+-----------------+
                 |                                   |
                 v                                   v
       +-------------------+              +-------------------+
       | TcpServerSocket   |              | TcpClientSocket   |
       | socket/bind/listen|              | socket/connect    |
       | accept_one        |              | local endpoint    |
       +---------+---------+              +---------+---------+
                 |                                  |
                 +---------------+------------------+
                                 |
                                 v
                            TCP stream

                         +------------------+
                         |    UdpSocket     |
                         | socket/bind      |
                         | send_to          |
                         | receive_from     |
                         +---------+--------+
                                   |
                                   v
                              UDP datagram
```

### `Ipv4Endpoint`

`Ipv4Endpoint` is the value object used at the networking boundary. It stores an IPv4 address and a
host-order `std::uint16_t` port.

Text parsing uses `inet_pton(AF_INET, ...)`. Formatting uses `inet_ntop(AF_INET, ...)`. Conversion
between the C++ value object and Linux `sockaddr_in` is centralized in:

```cpp
sockaddr_in Ipv4Endpoint::to_sockaddr() const noexcept;
static Ipv4Endpoint Ipv4Endpoint::from_sockaddr(const sockaddr_in& address);
```

The port conversion is intentionally localized at this boundary:

```text
host-order C++ port --htons()--> sockaddr_in::sin_port
sockaddr_in::sin_port --ntohs()--> host-order C++ port
```

Keeping byte-order conversion in one component prevents callers from repeatedly mixing host and
network byte order.

Port `0` is useful for tests and local tools. When a socket is bound with port `0`, Linux chooses an
ephemeral port. The socket classes call `getsockname()` afterward and expose the actual bound
endpoint.

## Runtime and lifecycle

### TCP server lifecycle

`TcpServerSocket` owns the listening descriptor.

```text
constructed
    |
    | start()
    v
socket(AF_INET, SOCK_STREAM)
    |
setsockopt(SO_REUSEADDR)
    |
bind()
    |
listen(SOMAXCONN)
    |
getsockname()
    v
listening
    |
    | accept_one()
    v
accepted UniqueFd
    |
    | stop()
    v
closed
```

`start()` is idempotent while the listener is already open. `stop()` closes the descriptor through
`UniqueFd::reset()` and clears the cached local endpoint.

`accept_one()` returns a new `UniqueFd`. This makes ownership of each accepted connection explicit:
the returned object owns that connected descriptor, while `TcpServerSocket` continues to own only
the listening descriptor.

The current server abstraction intentionally exposes one blocking `accept()` operation. A later
multithreading/event-loop phase can build concurrency around this boundary without changing the
basic ownership model.

### TCP client lifecycle

`TcpClientSocket` stores the intended remote endpoint and owns one connected socket descriptor.

```text
constructed
    |
    | connect()
    v
socket(AF_INET, SOCK_STREAM)
    |
connect(remote)
    |
getsockname()
    v
connected
    |
    | disconnect()
    v
closed
```

After a successful connection, `getsockname()` records the client-side local address and ephemeral
port. The remote endpoint remains the endpoint supplied to the constructor.

`connect()` is idempotent while already connected. `disconnect()` is safe to call repeatedly.

The Phase 3 class exposes `native_handle()` so integration tests and later protocol layers can use
standard stream operations without prematurely designing a message-framing abstraction. TCP is a
byte stream, so callers must not assume one `send()` maps to one `recv()`.

### TCP stream semantics in the integration tests

`tests/integration/test_tcp_network.cpp` deliberately contains helpers that loop until an entire
payload is transferred.

For sending:

```text
send remaining bytes
    |
    +-- EINTR --> retry
    |
    +-- 0 --> error: no progress
    |
    +-- n > 0 --> advance offset
```

For receiving:

```text
recv remaining bytes
    |
    +-- EINTR --> retry
    |
    +-- 0 --> peer closed before expected payload
    |
    +-- n > 0 --> advance offset
```

This demonstrates the important distinction between a TCP connection and an application message:
message boundaries must be defined by the application protocol, not inferred from individual
`send()`/`recv()` calls.

### UDP lifecycle

`UdpSocket` owns one datagram socket. It is explicitly bound before use.

```text
constructed
    |
    | bind()
    v
socket(AF_INET, SOCK_DGRAM)
    |
setsockopt(SO_REUSEADDR)
    |
bind()
    |
getsockname()
    v
bound
    |\
    | \ send_to(payload, destination)
    |  \
    |   receive_from(buffer)
    |         |
    |         +--> bytes_received
    |         +--> peer_endpoint
    |
    | close()
    v
closed
```

Unlike TCP, there is no listen/accept/connect handshake in this abstraction. Each `send_to()` names
a destination endpoint, and each `receive_from()` returns the sender endpoint with the datagram.

The payload API uses `std::span<const std::byte>` and `std::span<std::byte>`. This keeps the socket
layer binary-safe and avoids treating network payloads as null-terminated text. That is a useful
foundation for later RTP packets.

### UDP datagram semantics

UDP preserves datagram boundaries. A successful `send_to()` sends one datagram, and one
`receive_from()` consumes one datagram from the socket receive queue.

The Phase 3 integration tests verify:

```text
bidirectional datagram round-trip
binary payload preservation, including 0x00 and bytes above 0x7F
sender endpoint reported by recvfrom()
sequential datagrams received in the order observed on local loopback
```

The current abstraction returns the byte count reported by `recvfrom()`. It does not add a protocol
header, reliability, retransmission, ordering, duplicate suppression, or fragmentation policy.
Those concerns belong above this socket layer when a protocol requires them.

### Blocking I/O and `EINTR`

The Phase 3 sockets are blocking sockets. There is no `O_NONBLOCK`, `poll`, `select`, or `epoll`
integration yet.

Potentially interruptible operations are retried when Linux reports `EINTR` where the current code
needs that behavior:

```text
TCP connect
TCP accept
UDP sendto
UDP recvfrom
TCP integration-test send/recv helpers
```

Other failures are surfaced as `std::system_error` with the current `errno` and a contextual message.
Calls that violate object state, such as receiving on an unbound UDP socket, use `std::logic_error`.

### Ownership and exception safety

Temporary descriptors are first stored in local `UniqueFd` objects. The member descriptor receives
ownership only after socket setup succeeds.

For example, server setup follows this shape:

```text
local UniqueFd
    |
create/configure/bind/listen/query endpoint
    |
all operations successful
    v
move into member UniqueFd
```

If an intermediate operation throws, the temporary `UniqueFd` destructor closes the descriptor.
This is the central RAII property carried forward from Phase 2.

The socket wrapper classes themselves are non-copyable and non-movable in Phase 3. The accepted TCP
connection is transferable because `accept_one()` returns the movable `UniqueFd` directly.

## Verification strategy

### Test strategy

Unit tests focus on object state and local syscall behavior:

```text
tests/unit/test_ipv4_endpoint.cpp
tests/unit/test_tcp_server_socket.cpp
tests/unit/test_tcp_client_socket.cpp
tests/unit/test_udp_socket.cpp
```

Integration tests exercise real loopback traffic:

```text
tests/integration/test_tcp_network.cpp
tests/integration/test_udp_network.cpp
```

Loopback (`127.0.0.1`) and ephemeral ports keep tests self-contained. They do not depend on an
external server, internet access, or a fixed port that could already be in use.

## Design rationale

### TCP and UDP comparison in this repository

| Property | TCP path | UDP path |
| --- | --- | --- |
| Linux socket type | `SOCK_STREAM` | `SOCK_DGRAM` |
| Setup | server: bind/listen/accept; client: connect | bind |
| Data model | byte stream | datagrams |
| Message boundaries | not preserved by TCP | preserved per datagram |
| Peer discovery here | connected endpoint / `getpeername()` in tests | `recvfrom()` source endpoint |
| Phase 3 payload API | native fd used by integration tests | `std::span<std::byte>` API |
| Reliability added by this layer | no extra policy beyond TCP itself | none |
| Likely VoIP use later | signaling/control experiments | RTP/media foundation |

The final row describes project direction, not a claim that SIP is always TCP or that RTP is always
UDP. Later phases will choose transports according to their concrete protocol requirements.

## Phase boundary

Phase 3 does not attempt to solve:

```text
DNS or host-name resolution
IPv6
TLS
non-blocking sockets or epoll
TCP application framing
connection timeouts
socket cancellation
multi-threaded I/O ownership
SIP parsing/signaling
RTP packet structure, sequence handling, jitter, or audio
Qt event-loop integration
```

Keeping these concerns out of the socket wrappers makes the Phase 3 layer small enough to test and
reason about before the project introduces protocol and concurrency complexity.

## References

- `docs/phase3/README.md`
- `docs/interview/phase3-questions.md`
- `src/network/Ipv4Endpoint.hpp`
- `src/network/TcpServerSocket.hpp`
- `src/network/TcpClientSocket.hpp`
- `src/network/UdpSocket.hpp`
- `tests/integration/test_tcp_network.cpp`
- `tests/integration/test_udp_network.cpp`
