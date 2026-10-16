# Phase 10 — RTSP Mini-Lab

## Status

**Completed** — release identifier `v0.10-rtsp-lab`.

Phase 10 closes with the RTSP/SDP/RTP mini-lab, repeated loopback end-to-end verification, inherited quality gates, architecture/interview documentation, and concise sanitized evidence in place. The annotated tag is created only after closure review, merge to `main`, and post-merge verification; Git refs remain authoritative for publication state.

## Objective

Build a compact RTSP/1.0 learning lab that demonstrates the control-plane flow from TCP connection through `OPTIONS`, `DESCRIBE`, `SETUP`, `PLAY`, and `TEARDOWN`, then follows the negotiated SDP/Transport information far enough to receive and inspect RTP over UDP.

## Scope

Included in this phase:

- RTSP request/response models with `CSeq`, headers, body, serialization, and parsing;
- case-insensitive header lookup and `Content-Length` framing;
- RTSP TCP transport with bounded response size and complete-message reads;
- client state transitions for `OPTIONS`, `DESCRIBE`, `SETUP`, `PLAY`, and `TEARDOWN`;
- `Session` handling and `CSeq` response validation;
- SDP parsing for media lines, payload types, and `a=control` attributes;
- relative/absolute control-URI resolution;
- RTP-over-UDP `Transport` parsing for unicast client/server port pairs;
- RTP version-2 header parsing, including CSRC and extension-length accounting;
- UDP RTP reception with sequence-gap statistics;
- loopback end-to-end RTSP control and RTP reception tests;
- a Phase 10 verifier that inherits all earlier accepted project gates.

Deliberately excluded:

- RTSP authentication, redirects, proxies, and TLS;
- interleaved RTP over the RTSP TCP connection;
- RTCP parsing or report generation;
- codec depacketization, decoding, rendering, or audio playback;
- multicast transport;
- a general-purpose SDP parser;
- production integration with the Qt/PJSIP softphone path.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| Protocol model | Represent and validate RTSP requests/responses | `src/rtsp/RtspMessage.*`, `src/rtsp/RtspParser.*` |
| Control transport | Exchange one RTSP transaction at a time over TCP | `src/rtsp/RtspConnection.hpp`, `src/rtsp/RtspTcpConnection.*` |
| Session workflow | Enforce RTSP method ordering and session state | `src/rtsp/RtspClientSession.*` |
| SDP/Transport | Discover media control URI and UDP port information | `src/rtsp/SdpDescription.*`, `src/rtsp/RtspTransport.*` |
| RTP inspection | Parse RTP headers and track observed sequence gaps | `src/rtsp/RtpPacket.*`, `src/rtsp/RtpReceiver.*` |
| Verification | Unit, loopback integration, inherited regressions, static analysis, sanitizers | `tests/unit/test_rtsp_*.cpp`, `tests/integration/test_rtsp_*.cpp`, `scripts/phase10/` |
| Architecture note | Explain control/media separation and state flow | `docs/architecture/rtsp.md` |
| Interview review | Review protocol, transport, RTP, and design boundaries | `docs/interview/phase10-questions.md` |

## Architecture and design

The mini-lab keeps RTSP control and RTP media deliberately separate:

```text
RTSP control plane (TCP)
ClientSession
  -> OPTIONS
  -> DESCRIBE -> SDP
  -> SETUP -> Session + Transport
  -> PLAY
  -> TEARDOWN

RTP media plane (UDP)
SDP/control URI + SETUP port negotiation
  -> RtpReceiver bind
  -> RTP packet header parse
  -> sequence-gap statistics
```

`ClientSession` owns the RTSP state machine. A successful `DESCRIBE` moves the client to `Described`; successful `SETUP` captures the session identifier and moves to `Ready`; `PLAY` moves to `Playing`; `TEARDOWN` clears the session and returns to the connected state.

See `docs/architecture/rtsp.md` for the detailed design and phase boundaries.

## Build and run

Expose the established PJPROJECT 2.17 pkg-config path before running the inherited full gate:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase10/verify.sh
```

Run Phase 10 tests directly with:

```bash
cmake --preset debug
cmake --build --preset debug -j"$(nproc)"
ctest --test-dir build/debug -L phase10 --output-on-failure
```

Run the repeated loopback integration verification with:

```bash
./scripts/phase10/verify-integration.sh
```

## Verification

The Phase 10 gate covers protocol parsing/serialization, client-session state transitions, TCP loopback transactions, SDP and Transport parsing, RTP header parsing, UDP RTP reception, sequence-gap accounting, repeated loopback end-to-end scenarios, full inherited regressions, clang-format, clang-tidy, and sanitizer builds.

Observed full-suite and label counts are build observations rather than permanent API contracts; the verification scripts should remain authoritative instead of hard-coding historical totals into documentation.

## Evidence

The accepted sanitized evidence set is:

```text
evidence/phase10/
├── README.md
├── automated-verification.txt
└── integration-verification.txt
```

The retained summaries record the aggregate Phase 10 verifier, the loopback end-to-end RTSP/RTP contract, and the accepted branch CI outcome without retaining raw packet captures or host-specific build trees.

Do not commit credentials, authorization headers, private media, raw packet captures, or transient build artifacts.

## Interview focus

Be ready to explain:

- why RTSP is the control plane while RTP carries media;
- why `CSeq` and `Session` serve different purposes;
- how `DESCRIBE`/SDP leads to the URI used by `SETUP`;
- how `Transport` communicates RTP/RTCP UDP ports;
- why the mini-lab uses explicit client states;
- how TCP message framing differs from UDP RTP packet boundaries;
- what RTP sequence numbers, timestamps, payload type, marker bit, and SSRC mean;
- why a simple sequence-gap counter is not a complete RTP quality implementation;
- which production RTSP features are intentionally out of scope.

## Exit criteria

Phase 10 closure satisfies these criteria:

- RTSP request/response parsing and serialization are covered by tests;
- the TCP client validates complete responses, `CSeq`, and successful status codes;
- the session workflow enforces the intended method/state ordering;
- SDP media/control information is parsed and control URIs are resolved;
- RTP-over-UDP Transport information is validated;
- RTP version-2 headers are parsed and malformed inputs are rejected;
- UDP RTP reception and observed sequence-gap accounting are tested;
- loopback end-to-end RTSP control and RTP reception tests pass repeatedly;
- the complete Phase 10 verification gate passes;
- architecture, phase, interview, and sanitized evidence documents are present;
- no unsupported production-readiness claim is made beyond this mini-lab scope.

The release identifier is `v0.10-rtsp-lab`. The annotated tag is created only after closure review, merge to `main`, and post-merge verification.

## References

- `docs/architecture/rtsp.md`
- `docs/interview/phase10-questions.md`
- `src/rtsp/`
- `tests/unit/test_rtsp_protocol.cpp`
- `tests/unit/test_rtsp_client_session.cpp`
- `tests/integration/test_rtsp_media.cpp`
- `tests/integration/test_rtsp_end_to_end.cpp`
- `scripts/phase10/verify.sh`
- `scripts/phase10/verify-integration.sh`

## Next phase

After Phase 10 is accepted and `v0.10-rtsp-lab` is published, proceed to Phase 11 — Final Engineering Release. Do not start the next phase from the Phase 10 branch before closure, merge, and post-merge verification are complete.
