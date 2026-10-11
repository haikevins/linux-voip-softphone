# RTP Protocol — Controlled 75-second PCMU Study

## Status

**Observed in accepted Phase 6** (milestone `v0.7-rtp-audio`) during **one controlled approximately 75-second outgoing lab call**, captured and analyzed locally with Wireshark on 2026-09-30. The operator confirmed intelligible audio in both directions and reported an intentional silence interval of approximately 15 seconds near the end of the call. The sanitized per-direction record is `evidence/phase6/rtp-wireshark.txt`; the original capture is private and must not be committed.

## Purpose

Explain the on-wire RTP header and packet timing in the selected local-lab call, separating packet-level observations from the application's PJMEDIA stream counters and the operator's independent listening report.

## Scope

This study covers RTP version 2, the basic 12-byte header, G.711 PCMU (static payload type 0), sequence number, timestamp, SSRC, marker bit, and selected Wireshark loss/jitter summaries for two opposite network-facing RTP streams. It is not an end-to-end certification of all Asterisk media legs, networks, codecs, or devices. PJSUA2/PJMEDIA owns the call's RTP implementation; see `docs/architecture/audio-rtp.md`.

## Roles and terminology

- **RTP** transports time-sensitive media payloads over the negotiated transport, UDP in this lab.
- **Payload type (PT)** identifies the negotiated payload format. Static PT 0 is G.711 PCMU with an 8 kHz RTP clock.
- **Sequence number** is a 16-bit packet sequence field, useful for recognizing gaps and reordering; it wraps modulo 65536.
- **Timestamp** is a 32-bit media-clock value for the packet's first sample, not wall-clock time. Normal 20-ms PCMU packetization advances it by 160 ticks (8000 × 0.020).
- **SSRC** identifies an RTP synchronization source; values are capture-specific rather than SIP account identifiers.
- **Marker (M)** is payload-profile-dependent. A marked packet can indicate the first packet of a new audio talkspurt, but marker presence alone does not identify the precise media mechanism.
- **Jitter** estimates interarrival variation at the capture point; Wireshark's result is not identical to the softphone's PJMEDIA counter.
- **RTCP** reports/control information associated with RTP and is not the same as RTP audio packets.

## Wire model

The selected packets use RTP v2 without padding, extensions, or CSRC entries. The fixed header is 12 bytes:

```text
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |V=2|P|X| CC=0  |M|     PT=0     |        sequence number        |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           timestamp                           |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                              SSRC                             |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                      PCMU audio payload ...                   |
```

For selected frame 78, the capture size was 216 bytes with a 16-byte Linux cooked capture (SLL v1) header, a 20-byte IPv4 header, an 8-byte UDP header, and 172 bytes of UDP data: 12 bytes of RTP header plus 160 bytes of PCMU payload. These packet sizes describe the observed sample, not all possible RTP packets. The raw payload is intentionally excluded from the repository.

### Adjacent observed RTP packets

The following frame numbers and RTP header fields come from the single controlled study. Directions A and B denote the two selected opposite network-facing streams, not an assertion that every internal Asterisk media leg was captured.

| Direction | Frame | Sequence number | RTP timestamp | SSRC |
|---|---:|---:|---:|---|
| A → B | 78 | 0 | 1266780388 | `0xb7933d8d` |
| A → B | 80 | 1 | 1266780548 | `0xb7933d8d` |
| A → B | 82 | 2 | 1266780708 | `0xb7933d8d` |
| B → A | 14 | 31939 | 3217820739 | `0x1a2c9ab7` |
| B → A | 16 | 31940 | 3217820899 | `0x1a2c9ab7` |
| B → A | 18 | 31941 | 3217821059 | `0x1a2c9ab7` |

Normal adjacent packets in both sampled sequences advance the sequence number by 1 and RTP timestamp by 160; `160 / 8000 = 0.020 s`, or 20 ms of audio. Capture frame numbers interleave multiple observed streams and other traffic, so they need not be consecutive. Wireshark's *extended sequence number* is a derived analysis quantity, not an additional transmitted RTP header field.

## Runtime behavior

In the sampled PCMU streams, the RTP clock runs at 8 kHz and consecutive normally packetized 20-ms audio packets advance the RTP timestamp by 160. The sequence number advances for each **transmitted** RTP packet. An intentional silence interval need not appear as uninterrupted on-wire packets, and its duration cannot be inferred from the count of missing packet transmissions. Marker-bit meaning depends on the applicable RTP payload profile; it is not, by itself, proof of a particular silence-suppression configuration.

## Interactions and dependencies

SIP/SDP establishes a call and negotiates media; PJSUA2/PJMEDIA handles RTP, codec decoding, and routing to actual capture/playback devices. The application previously passed separate outgoing and incoming acceptance gates; this additional capture studies one controlled outgoing call. The operator reported actual two-way intelligible speech during this controlled test. The Wireshark statistics corroborate observed RTP packet flow but, alone, cannot prove audio was intelligible to either listener.

## Failure modes

Packet loss, packet reordering, interarrival jitter, and intentional or incidental media transmission pauses are distinct phenomena. A continuous sequence-number series does not demonstrate that packets were sent throughout an observed timestamp or arrival-time gap. A marker-delimited pause can be consistent with talkspurt behavior without identifying the cause. Wireshark's summaries describe packets observed by the capture point and cannot on their own establish subjective audio quality or rule out unobserved impairment on other media legs.

## Security considerations

The raw `.pcapng` contains network identifiers and potentially recoverable call audio because the observed media used PCMU. Keep it private outside the repository; never commit captures, unredacted screenshots, audio payloads, raw SIP logs, credentials, or Digest exchanges. This writeup retains only selected transient SSRC values, basic RTP fields, and aggregate measurements. The selected streams describe this one controlled call at the capture point, not every possible media path or operating environment.

## Verification observations

### Capture and reproduction method

1. Keep credentials out of command-line arguments. Configure the existing local Asterisk lab and establish the phone and workstation SIP registration using the project's normal procedures.
2. In Wireshark, use capture interface `any` and capture filter `udp portrange 10000-20000`, corresponding to the confirmed local Asterisk RTP port range. This filter deliberately omits ordinary SIP signaling and does not guarantee every negotiated RTP stream falls in that range.
3. Start capture before one outgoing test call and record the operator's observation. In this study the call lasted approximately 75 seconds, with intentional silence for approximately its final 15 seconds (operator-reported call timeline).
4. Stop capture. Wireshark may not infer RTP when the SIP/SDP negotiation is absent; use **Decode As → RTP** on the relevant UDP port where appropriate. Inspect the selected header fields and **Telephony → RTP → RTP Streams → Analyze** for each observed direction.
5. Keep the complete capture in a private, access-controlled folder **outside Git**. Record only selected numeric header fields and aggregate measurements after reviewing them for sensitive metadata.

### Observations from the 75-second study

These are Wireshark **RTP Stream Analysis** values for the two selected LAN-side, opposite-direction streams; neither is a universal measure of the entire call:

| Wireshark measure | Direction A → B | Direction B → A |
|---|---:|---:|
| SSRC | `0xb7933d8d` | `0x1a2c9ab7` |
| Analyzed/expected packets | 3,698 / 3,698 | 2,960 / 2,960 |
| Reported packet loss | 0 (0.00%) | 0 (0.00%) |
| Sequence errors | 0 | 0 |
| Mean jitter | 0.597943 ms | 0.464147 ms |
| Maximum jitter | 3.589688 ms | 0.784246 ms |
| Maximum delta in Analysis summary | 40.845230 ms | 21.954884 ms |
| Analyzed duration | 73.92 s | 72.92 s |
| Approximate RTP clock | 8,000 Hz | 8,000 Hz |

The private capture also has the initial direction A → B frame 78 (sequence 0), while the corresponding Wireshark analysis table starts at frame 80 (sequence 1). The analyzed packet total is quoted exactly as displayed; do not mix that count with a raw frame-level count.

### Intentional silence and marker-delimited pauses

The operator confirmed intelligible speech in both directions and deliberately remained silent for approximately seconds **60–75 of the call**. A separate reading of the direction B → A packet table shows several interarrival pauses. Near the end of the capture, overlapping the reported quiet interval, pauses include approximately **1.32 s** and two later gaps of approximately **5 s** each. At those points the sequence numbers remain continuous, RTP timestamps advance in accordance with the media clock, and the next transmitted packet is marked. The selected opposite A → B stream continued sending packets at near-20-ms intervals.

These are packet-level observations consistent with silence-related packet suppression or another media pause mechanism; they **do not prove** an exact vendor configuration or identify which sender's silence caused each pause. Fifteen seconds of planned operator silence is not one 15-second RTP packet gap, and the capture start time is not necessarily identical to the connected-call timeline. In direction B → A, Wireshark's 21.954884-ms *summary maximum delta* does not include all larger marker-delimited gaps shown in its packet table (including an approximately 1.54-second earlier pause). These are different views of the same observation, not contradictory packet-loss measurements. The zero-loss values mean no loss was detected within the analyzed stream intervals; they do **not** prove uninterrupted packet transmission or trouble-free audio in every condition.

## Phase boundary

The lab demonstrates the observed RTP header, timing, packet sequence, and opposite-direction stream statistics for one controlled study alongside the project's existing PJSUA2 diagnostics and audio acceptance tests. The accepted Phase 6 milestone does not establish performance under broader network impairment or certify other codecs, devices, and Asterisk media legs; those require independent tests and evidence.

## References

- RFC 3550 — RTP: A Transport Protocol for Real-Time Applications
- RFC 3551 — RTP Profile for Audio and Video Conferences with Minimal Control
- `docs/architecture/audio-rtp.md`
- `docs/phase6/README.md`
- `evidence/phase6/rtp-wireshark.txt`
