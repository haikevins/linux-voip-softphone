# Phase 10 Interview Review — RTSP, SDP, and RTP Mini-Lab

Use the repository implementation, documentation, tests, and evidence as the basis for the answers. Explain the protocol flow, framing decisions, state transitions, and verification boundaries rather than memorizing isolated RTSP or RTP definitions.

## Review scope

- RTSP/1.0 control flow over TCP
- `CSeq` transaction matching and `Session` lifecycle
- SDP media discovery and control-URI resolution
- RTP/AVP unicast UDP Transport negotiation
- RTP header parsing and sequence-gap statistics
- TCP message framing versus UDP datagram boundaries
- loopback integration verification and mini-lab boundaries

## Questions and answers

### RTSP control

#### What problem does RTSP solve in this lab?

RTSP controls a media session. It lets the client discover a presentation, configure media transport, start playback, and tear the session down. The media packets themselves are carried separately by RTP.

#### Why is RTSP sent over TCP here?

TCP gives the control channel reliable ordered bytes. The mini-lab then has to perform message framing itself by locating the RTSP header terminator and honoring `Content-Length` for the body.

#### What is `CSeq` used for?

`CSeq` identifies the sequence of RTSP requests and responses. The client increments it for requests and rejects a response whose `CSeq` does not match the request being processed.

#### What is the `Session` header used for?

After a successful `SETUP`, the server identifies the RTSP session with the `Session` header. The client retains that identifier and sends it with `PLAY` and `TEARDOWN`.

#### Why are `CSeq` and `Session` not the same thing?

`CSeq` correlates an individual request/response transaction. `Session` identifies the longer-lived media-control session established by `SETUP`.

#### Why does the client have explicit states?

The state machine prevents invalid method ordering. For example, `SETUP` requires a successful `DESCRIBE`, while `PLAY` requires a configured session. This makes protocol preconditions explicit rather than relying on scattered booleans.

### SDP and Transport

#### What does `DESCRIBE` provide?

The lab requests `application/sdp`. The SDP body describes media tracks and may provide `a=control` values that identify the URI to use for track-level RTSP control.

#### How is a relative control URI handled?

The lab resolves it against the RTSP base URI. An absolute `rtsp://` or `rtsps://` control URI is used directly; an empty or `*` control value falls back to the base URI.

#### What does the RTSP `Transport` header describe here?

It selects RTP/AVP over unicast UDP and communicates RTP/RTCP port pairs. The client advertises its client ports during `SETUP`; the parser can also read server port pairs from a response.

#### Why is RTP over RTSP/TCP interleaving excluded?

It would combine binary RTP frames with RTSP messages on one TCP stream and require an additional framing path. Phase 10 intentionally keeps the media plane on UDP so the control/media separation remains easy to inspect.

### RTP

#### What RTP fields does the mini-lab parse?

It validates RTP version 2 and extracts marker, payload type, sequence number, timestamp, SSRC, and the effective header size. It also accounts for CSRC entries and header extensions.

#### What does the RTP sequence number tell you?

It increments per RTP packet in a stream and can reveal forward gaps. The Phase 10 tracker uses those gaps as an observed-loss count.

#### What does the RTP timestamp tell you?

It represents the sampling timeline defined by the payload format. It is not wall-clock time and cannot by itself tell you when a packet arrived.

#### What is SSRC?

SSRC identifies a synchronization source within an RTP session. The mini-lab parses it but does not build a multi-source synchronization subsystem.

#### Is the sequence-gap counter a complete packet-loss algorithm?

No. It is intentionally simple. A production receiver would need to reason about reordering, duplicates, probation, extended sequence numbers, jitter, RTCP statistics, and other timing behavior.

### Networking and framing

#### Why can the RTSP TCP client not assume one `recv()` equals one response?

TCP is a byte stream. A response may be split across multiple reads or several bytes may arrive together. The implementation therefore buffers data until the header terminator and declared body length indicate a complete RTSP message.

#### Why does the mini-lab limit RTSP response size?

A network peer controls the incoming length. A fixed upper bound prevents unbounded buffering and turns unexpectedly large input into an explicit error.

#### Why does UDP RTP not need the same message-framing logic?

UDP preserves datagram boundaries. One successful `receive_from()` returns one datagram, which can then be parsed as one RTP packet for this lab.

### Testing and architecture

#### Why introduce a `Connection` interface?

It separates session-state behavior from socket behavior. Tests can inject deterministic responses into `ClientSession`, while `TcpConnection` and loopback integration tests independently validate the real network path.

#### What does the loopback end-to-end test prove?

It proves the implemented client can exchange the expected RTSP control sequence with a local TCP peer, preserve state/session contracts, and receive an RTP datagram on the UDP port negotiated by the lab scenario.

#### Why repeat the integration tests?

Socket and lifecycle bugs can be timing-sensitive. Repetition provides better evidence that the loopback scenario is stable than a single successful run, although it still does not prove all environments are race-free.

#### Why does Phase 10 inherit earlier verification gates?

New networking code must not silently regress the existing C++/Linux/network/SIP/media/Qt/threading/debugging work. The Phase 10 verifier is additive rather than replacing earlier acceptance checks.

#### What is the strongest boundary to state in an interview?

This is a protocol mini-lab, not a production RTSP media player. It demonstrates RTSP control, SDP/Transport interpretation, and RTP reception well enough to explain the end-to-end relationship while deliberately excluding authentication, RTCP, codecs, playback, interleaved RTP, and production integration.

## Phase boundary

Phase 10 is complete only when the documented RTSP/RTP path and verification evidence are accepted. The implementation should be presented as an educational, bounded protocol lab rather than as a feature-complete RTSP stack.

## References

- `docs/phase10/README.md`
- `docs/architecture/rtsp.md`
- `src/rtsp/`
- `tests/unit/test_rtsp_protocol.cpp`
- `tests/unit/test_rtsp_client_session.cpp`
- `tests/integration/test_rtsp_media.cpp`
- `tests/integration/test_rtsp_end_to_end.cpp`
- `scripts/phase10/verify.sh`
- `scripts/phase10/verify-integration.sh`
