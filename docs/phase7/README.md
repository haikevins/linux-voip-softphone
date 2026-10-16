# Phase 7 — Qt/QML GUI

## Status

**Completed for the documented local lab.**

Target milestone: `v0.8-qt-gui`. The annotated tag is created only after the Phase 7 branch is reviewed, merged into `main`, and the merged milestone state is verified.

Phase 7 adds a Qt 6/QML front end to the SIP/audio runtime built in Phases 5 and 6. The Phase 7 automated verifier passed the Qt-only build, Qt+PJSIP build, labeled controller/ViewModel tests, headless GUI launch smoke test, inherited regression/quality gates, and documentation/evidence structure checks. Manual GUI acceptance then confirmed registration, outgoing and incoming calls, visible state changes, clear two-way audio, call duration, and diagnostics against the controlled local lab. The accepted live runtime source revision was `090c278`; the closure commit changes evidence, documentation, metadata, and structure checks rather than the GUI call path.

## Objective

Provide an interactive Linux softphone UI without coupling QML directly to PJSIP. The UI should register an account, place and receive one active call, answer/hang up, display the current call state and duration, and expose simple diagnostics while reusing the previously verified SIP and audio path.

## Scope

Included in this phase:

- optional Qt 6/QML build support;
- a reusable application-level `SipEngine` abstraction;
- a long-lived `Pjsua2SipEngine` implementation for GUI registration and call control;
- `CallManager` ownership of the call-state machine and SIP-engine observer events;
- a QObject-based `CallViewModel` exposed to QML;
- account registration fields for username, password, server, ports, and UDP/TCP;
- outgoing dialing, incoming-call presentation, answer, hangup, failure/reset behavior, and call duration;
- simple state/account/peer/direction/duration diagnostics;
- deterministic controller/ViewModel tests and an offscreen GUI launch smoke test;
- a Qt-only configuration that keeps the GUI buildable without PJPROJECT.

Deliberately excluded from the Phase 7 implementation boundary:

- direct PJSIP/PJSUA2 calls from QML;
- the explicit worker-thread architecture planned for Phase 8;
- multi-call/conference orchestration;
- persistent account storage or a production credential store;
- new mute, audio-device-selection, microphone-level, or RTP-statistics controls in the QML screen;
- claiming live SIP/audio success from automated tests alone.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| Application interface | Decouple call control from PJSUA2 | `src/application/SipEngine.hpp` |
| Application controller | Own state transitions and map engine callbacks to domain events | `src/application/CallManager.*` |
| PJSUA2 backend | Keep one endpoint/account/call backend alive for the GUI session | `src/pjsip/Pjsua2SipEngine.*` |
| Qt ViewModel | Convert controller state/actions into QML properties and invokable methods | `src/gui/CallViewModel.*` |
| QML UI | Registration, dialer, incoming/active-call controls, diagnostics | `src/gui/Main.qml` |
| GUI bootstrap | Construct the backend factory and expose the ViewModel to QML | `src/gui/main.cpp` |
| Build presets | Build Qt-only and Qt+PJSIP variants | `CMakePresets.json`, `src/gui/CMakeLists.txt` |
| Automated tests | Verify controller/ViewModel behavior with fake engines | `tests/unit/test_call_manager.cpp`, `tests/unit/test_call_view_model.cpp` |
| GUI smoke test | Verify that the QML application stays alive offscreen | `scripts/phase7/verify-gui-smoke.sh` |
| Phase verifier | Run inherited gates plus Qt/GUI verification | `scripts/phase7/verify.sh` |
| Architecture note | Explain UI/application/backend boundaries and callback flow | `docs/architecture/qt-qml-gui.md` |
| Interview review | Review Phase 7 design decisions and limitations | `docs/interview/phase7-questions.md` |
| Evidence | Record sanitized manual/automated Phase 7 acceptance | `evidence/phase7/` |

## Architecture and design

The main dependency direction is:

```text
QML
 |
 v
CallViewModel (QObject)
 |
 v
CallManager
 |
 v
SipEngine interface
 |
 v
Pjsua2SipEngine
 |
 v
PJSUA2 / SIP / RTP / audio
```

QML sees only Qt properties and invokable methods on `CallViewModel`. It does not include, construct, or call PJSUA2 objects.

`CallManager` owns the existing domain state machine and translates UI requests into `SipEngine` operations. Engine callbacks are translated back into state-machine events such as registration success/failure, incoming call, remote ringing, connected, ended, and failed.

The visible state model remains:

```text
Idle
  -> Registering
  -> Ready
  -> Calling / Ringing
  -> Connected
  -> Terminating
  -> Ready

failures -> Failed -> reset/re-register
```

A PJSUA2 callback may arrive outside the Qt GUI thread. `CallManager` protects its state with a mutex, while `CallViewModel::on_state_changed()` schedules `refresh_from_manager()` with `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`. This avoids re-entering `CallManager` while its state mutex is held and moves the Qt-property refresh back through the ViewModel's event loop.

See `docs/architecture/qt-qml-gui.md` for the detailed ownership and callback model.

## Build and run

Qt support is optional. A Qt-only build is useful for compile/test/smoke verification without PJPROJECT:

```bash
cmake --preset qt-debug
cmake --build --preset qt-debug -j
build/qt-debug/src/gui/linux-voip-softphone-gui
```

In this configuration the UI launches, but registration intentionally reports that the PJSIP backend is not enabled.

For the real SIP backend, make the existing PJPROJECT 2.17 installation visible and use the combined preset:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

cmake --preset qt-pjsip-debug
cmake --build --preset qt-pjsip-debug -j
build/qt-pjsip-debug/src/gui/linux-voip-softphone-gui
```

The registration screen accepts:

- SIP username;
- password;
- SIP server/host;
- server port, normally `5060` in the documented local lab;
- local listen port, with `0` allowing PJSIP to choose an available port;
- UDP or TCP transport.

The password field is masked. Treat the password as runtime-only data: do not place it in source code, committed configuration, screenshots, terminal captures, or evidence text.

After registration reaches `Ready`, enter an extension with the on-screen dialer and place a call. For an incoming call, the same ViewModel exposes the ringing peer and enables Answer/Hangup actions according to controller state.

## Verification

Run the complete automated Phase 7 gate:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase7/verify.sh
```

The verifier performs:

- repository diff checks;
- the inherited Phase 6/PJSIP quality and regression gate;
- Qt-only configure/build and full regression;
- Phase 7-labeled `CallManager` and `CallViewModel` tests;
- a three-second offscreen QML launch smoke test;
- Qt+PJSIP configure/build and Phase 7 integration tests;
- documentation and evidence structure verification.

The automated gate proves deterministic state/action mapping, ViewModel behavior, Qt/PJSIP build integration, and basic GUI startup. It does **not** prove that a live Asterisk account registers, another endpoint rings, two-way audio is intelligible, or the expected state changes are visibly presented during a real call.

Before milestone closure, manually verify both directions against the controlled SIP lab:

```text
registration:  Idle -> Registering -> Ready
outgoing:      Ready -> Calling -> Ringing -> Connected -> Terminating -> Ready
incoming:      Ready -> Ringing -> Connected -> Terminating -> Ready
```

Observed networks/servers may skip or compress an intermediate visible ringing state, so acceptance should focus on correct final behavior and controller/backend logs rather than assuming every transient state must remain visible for a fixed duration.

## Evidence

The Phase 7 evidence namespace is:

```text
evidence/phase7/
```

The accepted Phase 7 evidence set contains:

```text
evidence/phase7/
├── README.md
├── automated-verification.txt
├── manual-verification.txt
├── runtime-media.txt
├── login.png
├── dialer.png
├── incoming-call.png
├── active-call.png
└── diagnostics.png
```

The five screenshots cover the masked registration screen, registered dialer, incoming-call controls, connected active call with duration, and connected-call diagnostics. The text artifacts retain concise automated, operator, and runtime-media results without committing raw SIP logs.

Evidence must remain sanitized. Do not commit:

- SIP passwords or other credentials;
- Digest `Authorization` headers;
- raw SIP/PJSIP logs that expose authentication material;
- SRTP keys or media keying material;
- raw packet captures;
- screenshots containing a visible password.

Automated verification output and screenshots should record the tested revision and what was observed without turning local account credentials into repository content.

## Interview focus

Use `docs/interview/phase7-questions.md` together with the implementation. Be ready to explain:

- why QML depends on a ViewModel rather than PJSUA2;
- the difference between `CallViewModel`, `CallManager`, and `SipEngine` responsibilities;
- why the GUI uses a long-lived backend instead of the Phase 5 one-shot CLI clients;
- how incoming and outgoing direction is represented while sharing one `Ringing` state;
- why PJSUA2 callbacks are queued before Qt-facing state is refreshed;
- why a Qt-only backend exists;
- how fake engines make controller/ViewModel tests deterministic;
- what the headless GUI smoke test proves and does not prove;
- why live SIP/audio verification remains separate from automated GUI tests;
- which concurrency work is intentionally deferred to Phase 8.

## Exit criteria

Phase 7 can be closed for the documented lab when:

- Qt/QML builds in both Qt-only and Qt+PJSIP configurations;
- QML controls call operations only through the ViewModel/controller/backend layers;
- registration, outgoing call, incoming call, answer, hangup, failure/reset, and duration state are represented correctly;
- the Phase 7 automated verifier passes;
- live registration and both call directions are exercised through the GUI;
- two-way audio from the reused Phase 6 media path is confirmed during the live GUI call;
- sanitized GUI/evidence artifacts are recorded without credentials;
- Phase 7 architecture/interview documentation and closure bookkeeping pass repository structure checks.

The recorded automated gate, live GUI registration/call flows, clear two-way audio, sanitized evidence set, and repository structure checks satisfy these exit criteria for the documented local lab. The target milestone is `v0.8-qt-gui`; the tag is created only after the Phase 7 branch is reviewed, merged into `main`, and the merged milestone state is verified.

## References

- Architecture: `docs/architecture/qt-qml-gui.md`
- Prior media architecture: `docs/architecture/audio-rtp.md`
- Call-state architecture: `docs/architecture/call-state-machine.md`
- Interview review: `docs/interview/phase7-questions.md`
- Automated verifier: `scripts/phase7/verify.sh`
- GUI smoke verifier: `scripts/phase7/verify-gui-smoke.sh`
- Evidence: `evidence/phase7/`

## Next phase

Phase 8 introduces the explicit multithreading/worker model. It should preserve the Phase 7 QML/ViewModel/application boundaries while moving blocking or long-running backend work behind deliberate thread-safe queues and synchronization rather than adding ad-hoc threading inside QML.
