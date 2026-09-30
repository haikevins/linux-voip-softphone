# Phase 7 — Qt/QML GUI

## Status

In progress. Commit 1 delivered the Qt dialer shell, Commit 2 the Qt-free `CallManager`, Commit 3 live SIP registration, Commit 4 outgoing SIP calls with two-way audio, Commit 5 incoming calls, and Commit 6 explicit SIP Decline plus connected-call duration. **Commit 7 adds read-only live RTP/audio diagnostics to the Qt GUI while keeping all PJSUA2 access on the retained worker.** Continuous registration-health monitoring, mute/device controls and Phase 7 closure remain future work.

## Objective

Expose the RTP/audio evidence already available in the Phase 6 PJSUA2 diagnostics layer without moving SIP/media ownership into QML. During a connected call, the GUI should show a small live diagnostic snapshot for the active audio stream: codec, RX/TX packet counts, packet loss, receive jitter and RTCP round-trip time.

## Scope

The combined `qt-pjsip-gui-debug` application started with `--sip-register USER SERVER [PORT]` can register, place and receive calls, answer/decline, show call duration, and now display live RTP/audio telemetry while the application state is `Connected`. Qt-only and GUI-preview modes remain network-free.

Diagnostics are presentation-only. They do not alter call state, SIP signaling, media routing or recovery behavior. If a diagnostic read fails temporarily, the call continues and the last successfully published snapshot remains visible until a later update or call teardown. Only the first active audio stream is presented because Phase 7 supports one tracked audio call.

## Deliverables

- `RegistrationSession` gains a Qt/PJSIP-neutral `RtpDiagnosticsSnapshot` read seam with a safe empty default for inert/fake sessions.
- `PjsipRegistrationSession` maps the existing `SipOutgoingCallClient::rtp_diagnostics()` result into that snapshot and sanitizes the codec label before it crosses the GUI boundary.
- `QtRegistrationBackend` samples diagnostics on the retained worker at most once per second after media connection and posts updates to the Qt thread with queued delivery.
- Diagnostic read failures are non-fatal and never tear down an otherwise healthy call.
- `GuiViewModel` exposes codec, RX/TX/lost packet counters, loss percentage, jitter and RTT as read-only QML properties and clears them when the call ends or fails.
- `Main.qml` adds a `Live RTP / Audio` panel visible only while a connected call has a published diagnostic snapshot.
- Qt fake-session tests verify worker-side sampling, UI-thread delivery, property values and deterministic clearing after disconnect.

## Architecture and design

```text
QML
  -> GuiViewModel                     (Qt UI thread)
  -> CallManager                      (Qt-free application state)
  -> CallBackend
  -> QtRegistrationBackend            (UI facade + retained std::jthread)
  -> RegistrationSession
  -> PjsipRegistrationSession
  -> SipOutgoingCallClient
       -> existing PJSUA2 RTP diagnostics
       -> active audio stream stats
```

The historical class name `SipOutgoingCallClient` remains because the retained Phase 6 client now owns both incoming and outgoing calls for the single account. Commit 7 does not introduce a second media/session owner.

`QtRegistrationBackend` samples only after `ConnectedWithMedia`. The worker reads the PJSUA2 stream statistics, converts them to the small `RtpDiagnosticsSnapshot`, and queues a signal to the Qt thread. QML never calls PJSUA2 directly and never receives PJSUA2 objects, SIP headers, credentials or raw remote URIs.

## Build and run

Build Qt-only and Qt+PJSIP separately. The production GUI keeps the existing hidden-password stdin flow; do not put a real SIP password in argv, logs or screenshots.

```bash
cmake --preset qt-gui-debug
cmake --build --preset qt-gui-debug --clean-first -j2

export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake --preset qt-pjsip-gui-debug
cmake --build --preset qt-pjsip-gui-debug --clean-first -j2
```

Start the combined build with the registered extension and SIP server using the existing `--sip-register USER SERVER [PORT]` interface, supplying the password through stdin rather than exposing it in shell arguments.

## Verification

Run the Qt-only tests, the combined Qt+PJSIP tests and the inherited Phase 6 gate:

```bash
ctest --preset qt-gui-debug \
    -R 'CallManagerTest|QtRegistrationBackendTest|QtGuiShellSmokeTest' \
    --output-on-failure

ctest --preset qt-pjsip-gui-debug \
    -R 'CallManagerTest|QtRegistrationBackendTest|QtGuiShellSmokeTest' \
    --output-on-failure

./scripts/phase6/verify.sh
```

The fake Qt diagnostic test uses deterministic values and proves worker-to-UI ownership only. Live values must still be checked during a real Asterisk call.

For the manual gate, establish either an outgoing or answered incoming call and confirm the `Live RTP / Audio` panel appears only after `Connected (audio active)`. Verify RX/TX counters increase during conversation, codec is populated, loss/jitter/RTT are finite non-negative values, and the panel disappears after local or remote hangup. Place another call without re-registering and confirm diagnostics repopulate for the new call.

## Evidence

Record only summarized PASS/FAIL, automated test counts, GUI-visible state and the small numeric RTP snapshot. Do not commit SIP passwords, Digest headers, packet captures containing credentials, raw audio or unredacted SIP logs.

The in-GUI values are an operational snapshot from PJSUA2/RTCP, not a substitute for Wireshark when packet-level evidence is required. A zero RTT or zero jitter early in a call may simply mean insufficient RTCP samples have accumulated yet.

## Interview focus

Be able to explain why RTP diagnostics are sampled on the existing SIP worker instead of from QML; why telemetry failures are non-fatal; why the GUI exposes a small sanitized snapshot rather than PJSUA2 structures; why the panel is gated by `Connected`; why counters are cleared at call teardown; and why the first active audio stream is sufficient for the current single-call scope.

## Exit criteria

Commit 7 is complete only when both Qt build variants and inherited Phase 6 regression pass, followed by a real Asterisk call showing live codec/counter/loss/jitter/RTT data, deterministic clearing after local/remote hangup, and diagnostics returning on a subsequent call without re-registration. Phase 7 remains open for the remaining GUI polish/health monitoring and milestone closure.

## References

- `README_roadmap.md` — Phase 7 Qt/QML GUI
- `docs/architecture/qt-qml.md`
- `docs/architecture/audio-rtp.md`
- `src/gui/QtRegistrationBackend.hpp`
- `src/gui/PjsipRegistrationSession.hpp`
- `src/pjsip/Pjsua2RtpDiagnostics.hpp`
- `src/pjsip/SipOutgoingCallClient.hpp`
- `tests/qt/test_qt_registration_backend.cpp`

## Next phase

Finish the remaining Phase 7 GUI reliability/polish work and then close the milestone. Keep continuous registration-health monitoring, mute/audio-device controls or broader concurrency changes as separately designed commits rather than mixing them into the RTP presentation path.
