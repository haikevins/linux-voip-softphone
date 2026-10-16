# Phase 10 Architecture — RTSP Control and RTP Mini-Lab

## Status

In progress — target milestone `v0.10-rtsp-lab`.

## Purpose

Document the Phase 10 RTSP mini-lab architecture, especially the separation between RTSP control over TCP and RTP media over UDP, the client state machine, protocol framing, SDP/Transport interpretation, and the deliberate limits of the implementation.

## Scope

This architecture covers `src/rtsp/`, the Phase 10 tests, and the Phase 10 verification scripts. It does not redefine the production SIP/PJSIP/Qt architecture from earlier phases.

Supported RTSP methods are `OPTIONS`, `DESCRIBE`, `SETUP`, `PLAY`, and `TEARDOWN`. The media side supports RTP/AVP over unicast UDP only.

## Architecture model

```text
Application / lab driver
        |
        v
ClientSession ---------------------------+
  state + CSeq + Session                 |
        |                                |
        v                                |
Connection interface                     |
        |                                |
        v                                |
TcpConnection                            |
        |                                |
        +---- RTSP request/response -----+---- TCP

DESCRIBE response body
        |
        v
SdpDescription -> control URI
        |
        v
SETUP Transport -> UDP port pair
        |
        v
RtpReceiver -> RtpPacket parser -> RtpStreamTracker
                                      |
                                      v
                               RtpStatistics
```

The protocol model is intentionally layered. `RtspMessage` represents wire-level messages, `RtspParser` handles syntax and framing rules, `TcpConnection` performs one request/response transaction, and `ClientSession` owns method ordering and session state.

The media helpers remain separate from `ClientSession`: SDP identifies a media/control target, `Transport` describes the selected UDP transport, and `RtpReceiver` receives datagrams and parses RTP headers.

## Runtime and lifecycle

A normal control flow is:

```text
Disconnected
    -> connect()
Connected
    -> OPTIONS              (state unchanged)
    -> DESCRIBE
Described
    -> SETUP(control URI)
Ready
    -> PLAY
Playing
    -> TEARDOWN
Connected
    -> disconnect()
Disconnected
```

Each request receives a monotonically increasing `CSeq`. `exchange()` rejects a response whose `CSeq` differs from the request and rejects non-2xx RTSP status codes. `SETUP` requires a `Session` header; parameters after the session identifier are not retained as part of the ID. `PLAY` and `TEARDOWN` send the negotiated session identifier.

TCP response handling waits for `\r\n\r\n`, reads `Content-Length` when present, and continues until the complete response is buffered. The mini-lab rejects duplicate `Content-Length`, responses larger than 1 MiB, premature peer close, and multiple pipelined responses in one transaction.

After `DESCRIBE`, the SDP helper parses media lines and `a=control` attributes. The first `RTP/AVP` or `RTP/AVP/UDP` track can be selected and its relative or absolute control URI resolved against the RTSP base URI.

`SETUP` requests unicast RTP/AVP with the configured client RTP/RTCP ports. The Transport parser accepts UDP RTP profiles and extracts optional client/server port pairs. TCP interleaving is intentionally rejected.

`RtpReceiver` binds a UDP socket, receives one datagram, parses the RTP version-2 header, and updates observed packet/loss counters. The header parser accounts for CSRC entries and RTP header extensions before reporting the payload offset.

## Verification strategy

Verification is layered:

- protocol unit tests cover method mapping, message serialization, header lookup, `CSeq`, `Content-Length`, and malformed input;
- client-session tests cover state ordering, session capture, status/CSeq failures, and one TCP loopback exchange;
- media tests cover SDP, Transport, RTP headers, sequence wrap/gaps, and UDP reception;
- end-to-end tests run a loopback RTSP server and exercise the complete control flow plus RTP reception and a SETUP failure path;
- `verify-integration.sh` repeats the loopback integration set to catch unstable behavior;
- `verify.sh` inherits all previous-phase gates and adds the Phase 10 checks;
- repository clang-format, clang-tidy, and sanitizer configurations remain mandatory.

The tests use loopback networking so the protocol behavior is exercised without depending on an external RTSP server or public media source.

## Design rationale

RTSP and RTP are separated because they solve different problems: RTSP controls a media session, while RTP carries time-sensitive media packets. Keeping separate classes makes the relationship explicit and prevents the control state machine from becoming a decoder/player implementation.

A `Connection` interface allows the session logic to be tested with deterministic fake responses while `TcpConnection` independently proves real TCP framing. This keeps state-machine tests fast and lets loopback integration tests cover the actual socket path.

The RTP tracker intentionally records only received packets and forward sequence gaps. It is an educational observation tool, not a complete RFC-grade receiver report implementation: it does not attempt jitter calculation, duplicate accounting, reordering windows, RTCP, codec timing, or playout quality.

The mini-lab uses bounded buffers and explicit error paths because RTSP is network input. Header injection, malformed numeric fields, inconsistent content lengths, invalid RTP versions, unsupported transport profiles, and invalid state transitions are rejected rather than silently accepted.

## Phase boundary

Phase 10 demonstrates enough RTSP/SDP/RTP behavior to explain and test the control-to-media path. It does not turn the repository into a general RTSP client or media player and does not alter the accepted production SIP softphone architecture.

Production-grade RTSP features such as authentication, TLS, redirects, proxy behavior, interleaved RTP, RTCP, codec depacketization/decoding, seek/range control, multiple simultaneous tracks, and long-running recovery are outside this phase.

## References

- `docs/phase10/README.md`
- `docs/interview/phase10-questions.md`
- `src/rtsp/RtspMessage.hpp`
- `src/rtsp/RtspParser.hpp`
- `src/rtsp/RtspClientSession.hpp`
- `src/rtsp/SdpDescription.hpp`
- `src/rtsp/RtspTransport.hpp`
- `src/rtsp/RtpPacket.hpp`
- `src/rtsp/RtpReceiver.hpp`
- `tests/phase10/`
