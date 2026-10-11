# Phase 6 — RTP and audio

## Status

Phase 6 acceptance is complete for the documented local lab configuration. Media implementation (Commit 7, `273ad69`), automated gate (Commit 8, `bbaec38`), architecture documentation (Commit 9, `c2354b3`), sanitized baseline verification evidence (Commit 10, `8337297`), and the single controlled approximately 75-second Wireshark RTP study (Commit 11, `4582baa`) have been reviewed together. Outgoing and incoming lab calls passed operator-confirmed audible two-way audio. Release identifier: `v0.7-rtp-audio`; the annotated Git tag is now published in repository refs.

## Objective

Extend the existing Phase 5 SIP call lifecycle to real Linux capture/playback, verified two-way audio, and read-only RTP/media diagnostics without regressing signaling or safe teardown.

## Scope

Phase 6 owns device capability preflight, stable capture/playback selection, active call-audio conference-bridge routing, RTP/RTCP statistics presentation, focused automated verification, and manual live-call validation. PJSUA2/PJMEDIA—not the project's general-purpose UDP socket class—owns the call's RTP transport and codecs. See `docs/architecture/audio-rtp.md` for component responsibilities and lifecycle detail.

## Deliverables

| Area | Implementation |
|---|---|
| Audio capability preflight | `scripts/phase6/verify-audio-environment.sh` |
| Capture/playback selection | `src/pjsip/Pjsua2AudioDeviceManager.*`, `src/pjsip/SipCallAudioConfig.hpp` |
| Two-way media routing | `src/pjsip/Pjsua2CallAudioBridge.*` and outgoing/incoming call media callbacks |
| RTP/media diagnostics | `src/network/RtpStatistics.*`, `src/pjsip/Pjsua2RtpDiagnostics.*` |
| CLI result reporting | `src/app/sip_call_main.cpp`, `src/app/sip_listen_main.cpp` |
| Automated verification | `scripts/phase6/verify.sh`, existing Phase 5 checks, focused GoogleTests |

These deliverables and their observed local verification outcomes are recorded for the stated lab configuration. Sanitized evidence is committed in Commits 10 and 11; success with another device or network requires separate validation.

## Architecture and design

SIP/SDP negotiates the call; PJSUA2/PJMEDIA supplies the negotiated call audio and RTP stream. After PJSUA2 starts, the call client resolves the requested audio devices by name and optional driver, then configures capture and playback. Once the media callback reports active audio, the bridge connects microphone-to-call and call-to-speaker with `AudioMedia::startTransmit()` in both directions.

The synchronous CLI waits for confirmed signaling and `media_connected`. While the call is still active, it reads stream information and PJMEDIA statistics, prints `[MEDIA]` and `[RTP]` snapshots, and only then requests hangup. The established Phase 5 callback synchronization and safe teardown sequence remain in use.

## Build and run

Expose the PJPROJECT installation and run the complete Phase 6 automated gate from the repository root:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase6/verify.sh
```

The wrapper starts with the audio preflight, runs `scripts/phase5/verify.sh` (including PJSIP/non-PJSIP regressions, ASan/UBSan, clang-tidy), then checks focused RTP/diagnostics suites. To build just the PJSIP applications:

```bash
cmake --preset pjsip-debug
cmake --build --preset pjsip-debug -j
build/pjsip-debug/src/voip-sip-call --help
build/pjsip-debug/src/voip-sip-listen --help
```

Supply the SIP password through the configured environment variable (the default CLI contract uses `VOIP_SIP_PASSWORD`); never pass it as a command-line argument. The examples below assume the user has already configured the existing local Asterisk lab and has securely set that variable in the active shell.

For the existing local `1002` (ThinkPad) and `1001` (phone) lab, test outgoing media with:

```bash
build/pjsip-debug/src/voip-sip-call \
    --username 1002 --server 127.0.0.1 --target 1001 --transport udp \
    --capture-device default --capture-driver ALSA \
    --playback-device default --playback-driver ALSA \
    --call-timeout-ms 30000 --hold-ms 20000
```

Test incoming media by starting the listener, then calling `1002` from the phone's `1001` account:

```bash
build/pjsip-debug/src/voip-sip-listen \
    --username 1002 --server 127.0.0.1 --transport udp \
    --capture-device default --capture-driver ALSA \
    --playback-device default --playback-driver ALSA \
    --incoming-timeout-ms 60000 --call-timeout-ms 15000 \
    --answer-delay-ms 1000 --hold-ms 20000
```

The stable selector `default` is resolved at runtime. Select an appropriate current Linux input/output device before making calls; an audio preflight PASS by itself does not confirm what a human will hear.

## Verification

**Automated gate:** `./scripts/phase6/verify.sh` must exit successfully. The Commit 8 verification run passed: PJSIP-focused 45/45, full PJSIP 117/117, sanitizer 117/117, non-PJSIP 72/72, RTP statistics 4/4, PJSUA2 RTP diagnostics 3/3, clang-tidy, and audio preflight. These counts describe that tested revision; later commits may add or reorganize cases.

**Manual live gate:** for both outgoing and incoming calls, confirm registration and call confirmation, actual two-way intelligible speech, `[MEDIA]` identifying an active negotiated codec, `[RTP]` with nonzero sent and received packets, and a clean disconnection. The Commit 7 reference runs observed PCMU at 8 kHz in both directions with zero reported receive loss; see `docs/architecture/audio-rtp.md` for the interpretation and limitations of these snapshots.

**Wireshark wire-level lab:** on 2026-09-30, one controlled approximately 75-second outgoing call was captured with a UDP RTP-port-range filter. The operator confirmed intelligible speech in both directions and reported deliberate silence during approximately seconds 60–75 of the call. Two selected opposite-direction streams showed PCMU, adjacent 160-tick/20-ms timestamp steps, and zero observed loss or sequence errors under Wireshark analysis. The selected network-facing stream in one direction transmitted regularly; the opposite direction contained marker-delimited pauses near the end of the capture, with continuous sequence numbers and matching timestamp advances. These observations are consistent with, but do not prove, silence-related media suppression. Results and limitations appear in `docs/protocols/rtp.md` and `evidence/phase6/rtp-wireshark.txt`. The Wireshark outgoing-call study does not replace the independently verified incoming-call manual gate.

A SIP `200 OK`, `audio=yes`, or nonzero RTP counts alone cannot prove intelligible sound through the chosen microphone and speaker. Live audio must be checked separately by a person.

## Evidence

The sanitized outcome record is indexed at `evidence/phase6/README.md`. Its short text artifacts retain the automated gate summary, registration observed during the live calls, outgoing/incoming codec and RTP measurements, operator-confirmed two-way listening, and a phase summary. The live calls were performed with Commit 7 (`273ad69`); later changes through Commit 9 added verification orchestration and documentation without changing the audio/RTP implementation. The additional `rtp-wireshark.txt` artifact records selected RTP headers and opposite-direction stream analyses from the controlled approximately 75-second packet-inspection lab. The artifacts are summaries of observed local runs, not synthetic call output or substitutes for future hardware-specific listening checks.

Do **not** commit passwords, Digest authorization headers, complete raw SIP/PJSIP logs, SRTP keying material, `.pcap`/`.pcapng` packet captures, or identifying information from unrelated peers. The 75-second capture remains private outside Git. Release tagging and branch merge were separate operations from the original acceptance evidence; the current repository refs now contain the published `v0.7-rtp-audio` milestone.

## Interview focus

Explain why a confirmed SIP dialog is different from audible media; how name/driver selection avoids fixed device IDs; what `onCallMediaState()` connects and why direction matters; why RTP statistics are read before hangup; how receive loss, jitter, and RTT are derived; and why automated tests, runtime counters, and actual listening are distinct verification layers. See `docs/interview/phase6-questions.md`.

## Exit criteria

Acceptance review is complete for the documented local lab: working two-way outgoing and incoming audio; valid codec/media and live RTP observations; clean disconnection; the complete Phase 6 automated gate; architecture, phase, interview, and RTP-protocol documentation; and sanitized evidence including the two selected Wireshark RTP directions from the controlled approximately 75-second outgoing call. These observations apply to the tested revision and environment; they are not a guarantee for other devices or network conditions. The annotated `v0.7-rtp-audio` tag is now published and should be verified via Git refs rather than inferred from this historical acceptance text.

## References

- `docs/architecture/audio-rtp.md`
- `docs/protocols/rtp.md`
- `docs/architecture/pjsip-cli.md`
- `docs/interview/phase6-questions.md`
- `docs/phase5/README.md`
- `scripts/phase6/verify.sh`
- `scripts/phase6/verify-audio-environment.sh`
- `evidence/phase6/README.md`

## Next phase

Phase 7 adds the Qt/QML user interface on top of the locally validated signaling and audio runtime, without changing the recorded Phase 6 acceptance results.
