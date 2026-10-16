# Phase 11 Demo Guide — Final Engineering Release

This guide defines a short, repeatable walkthrough for a reviewer, interviewer, or maintainer. It is a presentation guide, not release evidence and not a substitute for the Phase 11 verification scripts.

## Demo goals

The demo should make five engineering points visible:

1. the repository evolved through explicit, tagged milestones rather than one large final commit;
2. UI, application state, SIP/PJSIP ownership, media, and RTSP responsibilities remain separated;
3. the project has deterministic automated verification plus clearly separated live/manual evidence;
4. the final deliverable is tested after packaging, not only inside a development build tree;
5. `v1.0.0` is published only after merge and post-merge verification.

## Before the demo

Use a clean checkout on the intended revision:

```bash
git status --short
git log -1 --oneline --decorate
git tag --sort=version:refname
```

For the PJSIP installation used by the project:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
pkg-config --modversion libpjproject
```

Expected PJSIP version for the documented environment is `2.17`.

Do not preload or display SIP passwords, Digest headers, private captures, or unrelated peer data on screen.

## Recommended 10–15 minute walkthrough

### 1. Repository and milestone history

Show the top-level tree and tags:

```bash
find . -maxdepth 1 -mindepth 1 -printf '%f\n' | sort
git tag --sort=version:refname
```

Explain the progression:

```text
Modern C++
→ Linux systems programming
→ TCP/UDP networking
→ SIP/Asterisk
→ PJSIP CLI
→ RTP/audio
→ Qt/QML
→ threading/lifetime hardening
→ diagnostics/core dumps
→ RTSP/RTP mini-lab
→ final engineering release
```

Do not describe `v1.0.0` as published until the actual Git ref exists after Phase 11 closure.

### 2. Architecture boundaries

Open [`../architecture/system-overview.md`](../architecture/system-overview.md) and explain:

```text
QML
 ↓
view model / application state
 ↓
threaded SIP engine
 ↓
PJSUA2
 ↓
SIP + RTP/audio
```

Then point out that the RTSP mini-lab is separate:

```text
RTSP TCP control
      ↓
SDP discovery
      ↓
UDP RTP receiver
```

The important interview point is the boundary, not the number of classes.

### 3. Final automated/release verification

Run the aggregate gate when time allows:

```bash
./scripts/phase11/verify.sh
```

For a shorter package-focused demo:

```bash
./scripts/phase11/verify-package.sh
./scripts/phase11/verify-final-acceptance.sh
```

Explain that the second script exercises the **extracted package**, which catches install/package/runtime-dependency defects that a build-tree test can miss.

### 4. Inspect the release artifact

Show the generated archive:

```bash
find build/phase11-package -maxdepth 1 -type f -name '*.tar.gz' -print
```

Inspect its contents:

```bash
tar -tzf build/phase11-package/*.tar.gz
```

The expected executable set is:

```text
linux-voip-softphone
linux-voip-softphone-gui
rtsp-lab
voip-sip-register
voip-sip-call
voip-sip-listen
```

Explain why the project verifies package content and dynamic-library resolution before treating the artifact as a release candidate.

### 5. GUI and SIP behavior

If a graphical desktop and the controlled SIP lab are available, launch the GUI using the documented local configuration and demonstrate only the behavior that can be safely reproduced in the environment.

Useful points to show:

- registration state belongs to the application/backend path, not QML;
- outgoing and incoming call state transitions are explicit;
- Answer/Decline/Hangup are separate operations;
- connected-call duration is driven by call state;
- RTP/audio diagnostics are read-only presentation data;
- PJSUA2 work is kept away from the UI thread.

If the live lab is not available, do not fake a successful registration/call. Use the automated GUI/package smoke and historical sanitized evidence instead.

### 6. RTP/audio and diagnostics

Show the Phase 6 architecture/evidence surfaces and explain the distinction between:

```text
SIP dialog confirmed
≠
RTP packets observed
≠
audible two-way media confirmed by a human
```

Then show the Phase 9 diagnostics path: controlled crash generation, core-dump/GDB workflow, sanitizer coverage, and the repaired lifetime regression test. Emphasize that deliberate crash tooling is isolated from production behavior.

### 7. RTSP mini-lab

Run the packaged/help path or focused loopback verification:

```bash
./scripts/phase10/verify-integration.sh
```

Explain the flow:

```text
OPTIONS / DESCRIBE
        ↓
parse SDP control URI
        ↓
SETUP client UDP ports
        ↓
PLAY
        ↓
receive and parse RTP
        ↓
TEARDOWN
```

The lab demonstrates protocol parsing, state, TCP control, UDP media reception, and integration testing. It does not claim RTSP video/audio decoding or integration into the SIP GUI.

## Questions to be ready for

A reviewer is likely to ask:

- Why use PJSIP instead of implementing SIP from scratch?
- Why keep QML presentation-only?
- Why is PJSUA2 ownership/lifetime difficult during shutdown?
- What does ThreadSanitizer prove, and what does it not prove?
- Why verify an extracted package after CPack succeeds?
- Why keep the RTSP lab separate from the SIP call path?
- Why is `v1.0.0` tagged after merge rather than on the phase branch?

Use [`../interview/phase11-questions.md`](../interview/phase11-questions.md) for the deeper review set.

## Evidence boundary

A demo is not evidence by itself. The repository distinguishes:

- deterministic automated tests and quality gates;
- package-level smoke/acceptance;
- live SIP/audio observations in a controlled environment;
- sanitized committed evidence;
- final release/tag operations.

Never convert an unavailable live/manual check into a synthetic PASS just to make the demo look complete.

## End-of-demo release statement

Before Phase 11 closure, the correct statement is:

> The repository has a verified Phase 11 release-candidate workflow targeting `v1.0.0`; the final tag is created only after closure, merge, and post-merge verification.

After the closure commit is merged, the full gate passes on synchronized `main`, and the annotated tag is actually pushed, the statement may be updated to the published `v1.0.0` milestone.

## References

- [`README.md`](../../README.md)
- [`README.md`](README.md)
- [`../architecture/system-overview.md`](../architecture/system-overview.md)
- [`../interview/phase11-questions.md`](../interview/phase11-questions.md)
- [`../../scripts/phase11/verify.sh`](../../scripts/phase11/verify.sh)
- [`../../scripts/phase11/verify-package.sh`](../../scripts/phase11/verify-package.sh)
- [`../../scripts/phase11/verify-final-acceptance.sh`](../../scripts/phase11/verify-final-acceptance.sh)
