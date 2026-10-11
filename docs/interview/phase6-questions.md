# Phase 6 Interview Review — RTP and Linux Audio

Use the committed implementation and the local test observations as the basis for each answer. Distinguish what automated tests validate from what the manual Asterisk/handset call demonstrated.

## Review scope

Base each answer on the implementation, tests, and retained evidence. Distinguish design intent, observed behavior, and unverified boundaries.

- the distinction between SIP/SDP and the RTP/audio application path
- runtime audio-device selection and capability preflight
- bidirectional PJSUA2 conference-bridge wiring
- call-media callbacks, synchronization, and teardown
- RTP/RTCP diagnostic interpretation and verification limits
- automated versus live Phase 6 acceptance

## Questions and answers

### Signaling, devices, and media flow

#### Why did the Phase 5 null audio device not establish audible two-way audio?

Phase 5 established SIP registration, call signaling, callback synchronization, and cleanup without routing call media through an actual Linux microphone or speaker. A confirmed SIP dialog and negotiated audio stream do not independently verify capture-to-call and call-to-playback routing. Phase 6 adds that application-level bridge and checks it through a live call in both directions.

#### What is the responsibility boundary between SIP, SDP, RTP, and PJSUA2 conference media?

SIP establishes and terminates the dialog; SDP negotiates media parameters. PJSIP/PJMEDIA implements the negotiated stream, codec, RTP transport, and PJSUA2 conference media. The application selects the hardware-facing devices and connects their audio media to the call; its Phase 3 UDP abstraction is not a replacement RTP stack.

#### Why select audio devices by name and optional driver instead of a numeric ID?

Audio device enumeration can vary across machines, reboot cycles, and hot-plug events. The CLI supplies selectors such as `default` and `ALSA`; `Pjsua2AudioDeviceManager` enumerates current PJSUA2 devices and uses the core selection policy to resolve direction-capable capture/playback IDs at runtime. Numeric IDs are an output of discovery, not persistent configuration.

#### What does the audio-device preflight prove, and what does it not prove?

It verifies that PJPROJECT >= 2.17 is discoverable, a PJSUA2 audio program compiles and links, runtime enumeration succeeds, and at least one device supports each direction. It does not confirm that a particular headset is selected, that active call media is connected, or that another person can hear and be heard.

#### How is active call audio connected in both directions?

`Pjsua2CallAudioBridge` selects only `PJSUA_CALL_MEDIA_ACTIVE` entries of audio type. For each active index, it calls `call_audio.startTransmit(playback_media)` and `capture_media.startTransmit(call_audio)`. The first sends incoming call audio to playback; the second sends microphone samples into the call. Either missing edge can cause one-way application audio.

### Lifetimes and diagnostic interpretation

#### Why are `onCallState()` and `onCallMediaState()` handled separately?

Call state reports signaling transitions such as `CONFIRMED` or `DISCONNECTED`. Media state identifies an active stream that can actually be wired to audio devices. The CLI waits for confirmation and the client's `media_connected` result rather than declaring the audio path connected as soon as signaling succeeds. Media callback errors are recorded and reported.

#### How are statistics read without racing tracked-call teardown?

The client takes its call-callback lock around `rtp_diagnostics()`, then reads active stream information and statistics while the call is still alive. The CLI snapshots `[MEDIA]` and `[RTP]` before hangup. Teardown reuses Phase 5 serialization and waits for call disconnection and PJSUA call-slot cleanup before account/endpoint shutdown.

#### Where do `[RTP]` receive loss, jitter, and RTT come from?

The PJSUA2 adapter reads `getStreamInfo()` and `getStreamStat()`. The pure `RtpStatistics` helper computes `100 * lost / (received + lost)` using named receive/loss fields (zero when both counters are zero). The adapter converts PJMEDIA's mean receive jitter and mean RTCP RTT from microseconds to milliseconds; negative timing inputs are clamped to zero. These are library-provided snapshots, not independent packet captures.

#### Does `[RTP] lost=0` establish perfect network or audio quality?

No. A snapshot covers what the library reports at that time; some counters or RTCP timing estimates may not yet be populated. Even valid nonzero transmit/receive counters do not prove that the selected devices are audible or that speech is intelligible. Packet loss, jitter, audio routing, and human listening answer different questions.

### Verification and scope

#### What does the Phase 6 automated wrapper add to the established Phase 5 gate?

`scripts/phase6/verify.sh` runs the audio preflight, delegates to the full Phase 5 dependency/format/build/test/sanitizer/clang-tidy/non-PJSIP gate, and then discovers and runs dedicated RTP statistics (minimum four) and PJSUA2 diagnostics (minimum three) tests. Discovery count checks prevent an empty suite from appearing to pass. It cannot replace live Asterisk or handset interaction.

#### What constitutes the manual live gate?

Make and answer calls in both directions between the local account and the phone. For each direction, verify registration, a confirmed/media-connected call, actual two-way intelligible speech, negotiated `[MEDIA]` output, meaningful sent/received RTP counters, and clean disconnection. Treat the values from a successful lab run as observations, not permanent performance targets.

#### What evidence is safe to commit for this milestone?

Only reviewed summaries: revision, test gate results, call direction/state, codec and selected counters, human confirmation of bidirectional audio, and clean teardown. Never commit SIP credentials, Digest headers, raw SIP/PJSIP traces, keying material, or `.pcap`/`.pcapng` captures. The Phase 6 milestone was subsequently accepted; see `evidence/phase6/README.md` for the retained sanitized observations.

## Phase boundary

Phase 6 extends the existing one-shot CLI from SIP signaling to validated physical audio routing and diagnostic visibility. It did not establish a long-running multi-call controller or production security policy. Qt/QML UI work was addressed separately by the accepted Phase 7 milestone.

## References

- `docs/phase6/README.md`
- `docs/architecture/audio-rtp.md`
- `docs/architecture/pjsip-cli.md`
- `src/pjsip/Pjsua2AudioDeviceManager.*`
- `src/pjsip/Pjsua2CallAudioBridge.*`
- `src/pjsip/Pjsua2RtpDiagnostics.*`
- `src/network/RtpStatistics.*`
- `scripts/phase6/verify.sh`
