# Phase 3 Networking Interview Review

Use the repository implementation as the example. The goal is to explain behavior and trade-offs,
not to memorize one-line definitions.

## What is the difference between TCP and UDP in this project?

TCP uses `SOCK_STREAM` and establishes a connection. The server binds/listens/accepts while the
client connects. Application message boundaries are not preserved, so the integration test loops
around `send()` and `recv()` until the expected byte count is transferred.

UDP uses `SOCK_DGRAM`. `UdpSocket` binds a local endpoint, `send_to()` names a destination for each
datagram, and `receive_from()` returns both the number of bytes received and the sender endpoint.
Datagram boundaries are preserved by the socket API.

Repository references:

```text
src/network/TcpServerSocket.cpp
src/network/TcpClientSocket.cpp
src/network/UdpSocket.cpp
tests/integration/test_tcp_network.cpp
tests/integration/test_udp_network.cpp
```

## Why does `Ipv4Endpoint` use `htons()` and `ntohs()`?

The C++ port value is represented in host byte order. `sockaddr_in::sin_port` uses network byte
order. `to_sockaddr()` converts with `htons()`, while `from_sockaddr()` converts back with
`ntohs()`.

Keeping this conversion inside `Ipv4Endpoint` reduces the chance that individual socket callers
mix the two representations.

Repository reference: `src/network/Ipv4Endpoint.cpp`.

## What do `inet_pton()` and `inet_ntop()` do?

`inet_pton()` parses the textual IPv4 form into the binary address representation used by socket
APIs. `inet_ntop()` converts the binary IPv4 address back to printable text.

The implementation rejects an empty or malformed textual address and uses `AF_INET`, so Phase 3 is
explicitly IPv4-only.

## Why do tests bind to port `0`?

Port `0` asks the kernel to select an available ephemeral port. This avoids hard-coded ports that
may already be occupied. After `bind()`, the code calls `getsockname()` to discover the actual
assigned endpoint.

Repository references: `TcpServerSocket::start()`, `UdpSocket::bind()`, and their unit/integration
tests.

## Why can one TCP `send()` require multiple `recv()` calls?

TCP exposes a byte stream, not records. A successful `send()` reports how many bytes that call
accepted, and `recv()` reports bytes currently delivered to the receiver. Neither call defines an
application-level message boundary.

The integration test therefore implements `send_all()` and `receive_exact()` loops. In a real
protocol, framing might use a fixed header, delimiter, or length field.

Repository reference: `tests/integration/test_tcp_network.cpp`.

## What does `accept()` return and who owns it?

`accept()` creates a new connected descriptor for one client. The listening descriptor remains
open for future clients.

`TcpServerSocket::accept_one()` wraps the accepted descriptor in `platform::UniqueFd` and returns it
by value. The caller then owns that connection through RAII.

Repository reference: `src/network/TcpServerSocket.cpp`.

## Why is `UniqueFd` important for socket code?

File descriptors are resources that must be closed exactly once. `UniqueFd` gives one object
exclusive ownership and closes the descriptor from its destructor/reset path.

During socket setup, local `UniqueFd` objects own temporary descriptors. If `setsockopt`, `bind`,
`listen`, `connect`, or `getsockname` throws, stack unwinding destroys the temporary owner and
closes the descriptor.

This avoids scattered manual `close()` calls and reduces leak/double-close risk.

## Why does the code call `getsockname()`?

For listeners and UDP sockets, it discovers the actual local endpoint after `bind()`, especially
when the requested port was `0`.

For the TCP client, it discovers the local address and ephemeral port selected for the established
connection.

Repository references: all three socket implementations.

## What is `SO_REUSEADDR` doing here?

The TCP server and UDP socket set `SO_REUSEADDR` before binding. In this phase it is a socket setup
policy applied before `bind()` so local development/tests can reuse addresses according to Linux
socket semantics.

Do not describe it as permission for arbitrary processes to bind the exact same TCP listening
address/port simultaneously; that is not what the Phase 3 code demonstrates.

## What is `EINTR`, and how does the code handle it?

A blocking system call can return `-1` with `errno == EINTR` when interrupted before completing.
The current implementation retries selected operations in loops:

```text
connect
accept
sendto
recvfrom
```

The TCP integration-test `send()`/`recv()` helpers also retry on `EINTR`. Other failures become
`std::system_error`.

## Why does UDP use `std::byte` instead of `char` strings?

Network datagrams can contain arbitrary binary data. `std::span<const std::byte>` communicates a
non-owning binary input buffer, while `std::span<std::byte>` represents a writable receive buffer.

The UDP integration test includes `0x00`, `0x80`, `0xFE`, and `0xFF` values to verify binary payload
preservation.

Repository references: `src/network/UdpSocket.hpp` and
`tests/integration/test_udp_network.cpp`.

## Does UDP guarantee delivery or ordering here?

No reliability layer is implemented in `UdpSocket`. It sends and receives datagrams using the
kernel socket API and reports the observed sender endpoint and byte count.

The local integration test verifies sequential datagrams on loopback, but that test must not be
presented as an application-level reliability or ordering guarantee for arbitrary networks.

## Why are the integration tests on loopback useful?

They exercise real kernel sockets and real TCP/UDP system calls while staying self-contained. They
verify interactions that isolated unit-state tests cannot prove, including connect/accept,
peer/local endpoint relationships, stream round-trips, and datagram source endpoints.

They are not substitutes for later tests across real devices or networks when those scenarios
become part of a phase requirement.

## What is intentionally missing from Phase 3?

Be ready to say that the current networking layer does not yet provide:

```text
IPv6
DNS resolution
TLS
non-blocking I/O or epoll
timeouts/cancellation
multi-threaded socket orchestration
TCP message framing as a reusable production abstraction
SIP parsing or transactions
RTP headers, sequencing, jitter handling, or audio
Qt event-loop integration
```

This is deliberate scope control. Phase 3 establishes a tested socket foundation before those
higher-level concerns are added.
