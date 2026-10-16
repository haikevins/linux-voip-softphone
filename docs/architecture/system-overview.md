# Phase 11 Architecture — Final System Overview

## Status

**In progress** — target milestone `v1.0.0`.

This document consolidates the architecture already implemented and verified across Phases 0–10. It is a map of the final system and release boundaries, not a new runtime layer.

## Purpose

The final repository spans raw Linux/networking foundations, SIP/PJSIP call control, RTP/audio, a Qt/QML presentation layer, explicit threading/ownership rules, diagnostics, and an RTSP/SDP/RTP mini-lab. The purpose of this overview is to show how those parts fit together without erasing the boundaries that made each phase independently testable.

A reader should be able to answer three questions from this document:

- Which layer owns each class of state and side effect?
- Which runtime path handles a SIP call versus an RTSP lab session?
- Which verification layer proves source behavior, packaged behavior, and release readiness?

## Scope

This overview covers:

- the production softphone path from Qt/QML to PJSUA2 and RTP/audio;
- worker-thread ownership and queued handoff;
- Linux/networking foundations reused by higher layers;
- diagnostics and controlled debugging boundaries;
- the separate RTSP control/RTP lab path;
- build, install, package, verification, and release flow.

It does not redefine protocol details already documented in the phase-specific architecture/protocol documents.

## Architecture model

### Layered production path

```text
+-------------------------------------------------------------+
| Qt/QML presentation                                        |
|  - user input                                               |
|  - rendered call/registration state                         |
|  - read-only diagnostics display                            |
+------------------------------+------------------------------+
                               |
                               | Qt-facing model / queued events
                               v
+-------------------------------------------------------------+
| Application control                                        |
|  - call state                                               |
|  - validation                                               |
|  - application-level transitions                            |
+------------------------------+------------------------------+
                               |
                               | SipEngine abstraction
                               v
+-------------------------------------------------------------+
| Threaded SIP/PJSIP boundary                                 |
|  - serialized work                                          |
|  - retained endpoint/account/call ownership                 |
|  - callback handoff                                         |
|  - explicit shutdown/lifetime policy                        |
+----------------------+----------------------+---------------+
                       |                      |
                       | SIP signaling        | media callbacks
                       v                      v
              +----------------+      +-----------------------+
              | PJSUA2 / SIP   |      | RTP / Linux audio     |
              | register/call  |      | device + statistics   |
              +----------------+      +-----------------------+
```

The UI does not own PJSUA2 objects. Application state does not depend on QML implementation details. PJSUA2 work is kept behind the threaded engine/session boundary so callbacks and object lifetime are not managed ad hoc from the UI thread.

### Networking foundation

Earlier networking phases supply the engineering foundation for endpoint validation, TCP/UDP semantics, process behavior, and testable socket lifecycles. Higher-level SIP/RTSP components may use operating-system/network APIs directly where appropriate, but the repository keeps protocol-specific state machines separate from generic socket concepts.

### SIP and RTP/audio path

The production softphone path is:

```text
Qt action
  → application request
  → worker-owned SIP engine
  → PJSUA2 endpoint/account/call
  → SIP signaling through Asterisk/peer
  → media state callback
  → Linux capture/playback connection
  → read-only RTP/media diagnostics
  → queued state/diagnostic update to Qt
```

SIP dialog state and audible media are deliberately verified as different layers. A confirmed call does not by itself prove audio, and RTP counters do not substitute for operator listening evidence where a live audio gate is required.

### RTSP mini-lab path

The RTSP mini-lab remains a sibling subsystem:

```text
rtsp-lab
  → RTSP TCP connection
  → OPTIONS / DESCRIBE / SETUP / PLAY / TEARDOWN
  → SDP media/control discovery
  → RTP/AVP UDP receiver
  → RTP header/statistics inspection
```

It shares protocol/networking concepts with the broader project but does not become a media source for the SIP softphone in Phase 11. Keeping the lab separate preserves the scope accepted in Phase 10.

### Cross-cutting engineering boundaries

The final system relies on several cross-cutting policies:

- **ownership:** endpoint/account/call and callback lifetimes are explicit rather than shared implicitly across UI code;
- **threading:** serialized worker execution and queued handoff isolate backend work from Qt presentation;
- **failure handling:** expected invalid input becomes controlled errors; deliberate crash scenarios are isolated to diagnostic tooling;
- **verification:** unit, integration, stress, sanitizer/static-analysis, live/manual evidence, and package-level smoke checks answer different questions;
- **release:** generated artifacts are recreated from source and are not treated as source-of-truth state.

## Runtime and lifecycle

### Application startup

The final GUI runtime follows this ownership sequence conceptually:

```text
process start
  → construct Qt application/presentation objects
  → construct application controller
  → construct/start threaded SIP backend as requested
  → PJSUA2 endpoint/account setup
  → publish state to UI
```

Configuration and credential handling remain outside committed evidence. Help/smoke paths must not require real SIP credentials.

### Registration and call lifecycle

The accepted production lifecycle is:

```text
unregistered
  → registration requested
  → registration callback result
  → ready
  → outgoing or incoming call
  → ringing / answer or decline
  → connected
  → media active
  → hangup / remote disconnect
  → teardown call object
  → ready or stopped
```

The application layer exposes only transitions justified by backend events. Unexpected callbacks must not fabricate a successful state.

### Thread and callback lifecycle

PJSUA2 callbacks can arrive independently of Qt UI execution. The system therefore uses explicit backend ownership, guarded callback/lifetime policies, and queued handoff to the presentation side. Shutdown is an engineering state transition, not merely object destruction; accepted work is drained or rejected according to the worker policy and backend teardown must complete before owned objects are released.

### Release artifact lifecycle

Phase 11 adds a separate artifact lifecycle:

```text
clean source checkout
  → CMake release preset
  → Release build
  → install staging
  → CPack TGZ
  → extract into temporary acceptance root
  → dependency audit
  → packaged CLI smoke
  → packaged Qt offscreen smoke
  → closure evidence
  → merge to main
  → post-merge verification
  → annotated v1.0.0 tag
```

The tag is not a build input and is not created from the phase branch. It records the verified post-merge release point.

## Verification strategy

### Source and regression verification

`scripts/phase11/verify.sh` inherits `scripts/phase10/verify.sh`, which in turn carries the prior phase gates. This keeps the final release tied to the accumulated project regression rather than replacing historical checks with a single packaging test.

### Release build verification

`scripts/phase11/verify-package.sh` verifies the Release preset, install staging, expected executable inventory, and TGZ package creation. This answers whether the project can be assembled into a release artifact from the current source tree.

### Packaged-system acceptance

`scripts/phase11/verify-final-acceptance.sh` tests the extracted package. It checks path safety, runtime dependencies, CLI help/smoke behavior, and bounded offscreen Qt startup. This is intentionally downstream of CPack so it can catch missing install rules or runtime library dependencies that a normal build-tree test may miss.

### CI and post-merge verification

Per-commit CI must be green before the next Phase 11 commit. The closure commit still does not publish `v1.0.0`; after merge, synchronized `main` must rerun the full Phase 11 gate. Only then is the annotated tag created and pushed.

## Design rationale

### Preserve boundaries rather than creating a "god" final layer

A final release does not justify collapsing QML, application state, PJSUA2 ownership, media, or RTSP logic into one executable-level abstraction. The existing boundaries make failures local, tests focused, and explanations credible.

### Verify the package, not only the build tree

A build-tree executable can pass while install rules omit files or package runtime dependencies are broken. Extracting and running the TGZ checks the deliverable that a reviewer or user would actually receive.

### Keep live/manual evidence distinct from deterministic automation

Automated regression, package smoke, live SIP registration/call behavior, and audible media checks have different confidence scopes. Phase 11 must summarize them without claiming that one category proves another.

### Tag after merge and post-merge verification

The release tag should identify the exact `main` commit that was actually verified after integration. Creating `v1.0.0` earlier would make the tag describe an intent rather than an accepted release point.

## Phase boundary

Phase 11 may improve release mechanics, documentation, verification orchestration, and presentation of already accepted functionality. It must not quietly expand product scope.

Not part of this milestone unless a new roadmap explicitly authorizes it:

- multiple simultaneous calls or accounts;
- call waiting, conferencing, mute, or hold;
- new codec/media-processing pipelines;
- SRTP/TLS deployment hardening;
- RTSP decode/playback integration into the SIP GUI;
- production packaging for every Linux distribution;
- claims of universal hardware/network compatibility.

The intended result is a coherent engineering release of the documented project, not a commercial softphone product claim.

## References

- [`../../README_roadmap.md`](../../README_roadmap.md)
- [`../phase11/README.md`](../phase11/README.md)
- [`networking.md`](networking.md)
- [`sip-asterisk.md`](sip-asterisk.md)
- [`pjsip-cli.md`](pjsip-cli.md)
- [`audio-rtp.md`](audio-rtp.md)
- [`qt-qml.md`](qt-qml.md)
- [`threading-model.md`](threading-model.md)
- [`debugging-diagnostics.md`](debugging-diagnostics.md)
- [`rtsp.md`](rtsp.md)
- [`../interview/phase11-questions.md`](../interview/phase11-questions.md)
- [`../../scripts/phase11/verify.sh`](../../scripts/phase11/verify.sh)
