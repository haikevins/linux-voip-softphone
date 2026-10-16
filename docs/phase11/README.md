# Phase 11 — Final Engineering Release

## Status

**In progress** — target milestone `v1.0.0`.

Phase 11 does not add a new telephony subsystem. It turns the accepted Phase 0–10 work into one reviewable engineering release with a reproducible Release build, packaged executables, final system acceptance, consolidated architecture/operations documentation, portfolio-facing presentation, and closure evidence.

## Objective

Produce a final release candidate that can be built, packaged, inspected, and exercised from a clean checkout without changing the previously accepted SIP, RTP/audio, Qt/QML, threading, diagnostics, or RTSP behavior.

The phase must make the final repository easy to evaluate in two ways: an engineer can follow one verification path from source to package, and an interviewer can trace each major design claim to code, tests, documentation, and evidence.

## Scope

Phase 11 covers:

- the CMake `release` preset with Qt/QML and PJSIP enabled;
- install rules and a TGZ package for the final application executables;
- package-level dependency and smoke verification;
- final system acceptance that inherits the complete Phase 10 verification chain;
- a consolidated architecture and runtime/operations view;
- final README/demo presentation and release evidence in later commits;
- post-merge verification before the annotated `v1.0.0` tag is created.

The phase does **not** use final-release work as an excuse to add call waiting, multiple accounts, mute/hold, new codecs, SRTP, RTCP control, RTSP decoding/playback, or other unplanned product features.

## Deliverables

At the end of Phase 11 the repository is expected to provide:

- `scripts/phase11/verify.sh` as the aggregate Phase 11 entry point;
- `scripts/phase11/verify-release-scope.sh` for release-baseline checks;
- `scripts/phase11/verify-package.sh` for reproducible Release build/install/package verification;
- `scripts/phase11/verify-final-acceptance.sh` for package-level system acceptance;
- a CPack TGZ release package carrying the final executable set;
- `docs/architecture/system-overview.md` as the final cross-phase architecture map;
- `docs/interview/phase11-questions.md` as the final engineering-release review;
- [`demo-guide.md`](demo-guide.md) as the final portfolio/reviewer walkthrough;
- `evidence/phase11/` and completed release status in Commit 6.

## Architecture and design

The final system keeps the boundaries established by earlier phases instead of flattening them for release:

```text
Qt/QML presentation
        ↓
application state / call control
        ↓
threaded SIP engine boundary
        ↓
PJSUA2 / SIP signaling ───────────────┐
        ↓                              │
RTP / Linux audio                     │
                                       │
RTSP mini-lab ── TCP control + UDP RTP┘

cross-cutting:
  ownership / shutdown policy
  diagnostics / regression tooling
  CMake presets / packaging / CI
```

The RTSP mini-lab remains a separate protocol-learning surface; it is packaged with the project, but it is not silently merged into the SIP call path.

See [`../architecture/system-overview.md`](../architecture/system-overview.md) for the complete component, runtime, verification, and release-boundary view.

## Build and run

The development verification path remains available through the inherited presets. Phase 11 adds the release path:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

cmake --preset release
cmake --build --preset release -j"$(nproc)"
./scripts/phase11/verify-package.sh
```

The package verifier performs a fresh release configure/build, install staging, CPack TGZ generation, and package-content checks. The final acceptance script then extracts that package and exercises the packaged programs rather than relying only on binaries from a developer build tree.

Final aggregate verification is:

```bash
./scripts/phase11/verify.sh
```

Do not create the `v1.0.0` tag from the phase branch. The release tag is a separate post-merge operation after current `main` passes the Phase 11 gate.

## Verification

Phase 11 verification is layered deliberately:

1. **Inherited engineering regression** — `scripts/phase10/verify.sh` preserves the accumulated Phase 6–10 quality/test chain.
2. **Release-scope verification** — checks the expected Phase 10 milestone baseline and repository layout before final-release work proceeds.
3. **Release package verification** — proves that the Release configuration can build, install, and create the expected TGZ artifact.
4. **Packaged-system acceptance** — extracts the TGZ, audits runtime dependencies, runs CLI help/smoke checks, and launches the Qt GUI through an offscreen bounded smoke test.
5. **Closure verification** — Commit 6 will freeze sanitized evidence, update current-facing status, merge to `main`, rerun the gate, and only then publish `v1.0.0`.

A green package smoke test proves the documented packaged behavior in the tested environment; it is not a substitute for live SIP credentials, external audio hardware, or arbitrary deployment certification.

## Evidence

Phase 11 evidence is intentionally deferred to the closure commit. Until Commit 6, do not create final-looking evidence that claims `v1.0.0` is already published.

The final evidence set should record only sanitized, reproducible outcomes such as:

- aggregate automated verification result;
- release package name and package-content verification;
- packaged CLI/GUI smoke result;
- CI result for the closure commit;
- post-merge verification result;
- final Git tag/release identifier after publication.

Secrets, SIP passwords, Digest headers, private packet captures, and machine-specific credentials must not be committed.

## Interview focus

Be ready to explain why a final release is more than a successful Debug build; why the packaged artifact must be exercised after extraction; why `v1.0.0` is created only after merge and post-merge verification; how the Qt/application/PJSIP/threading boundaries reduce ownership risk; and why the RTSP mini-lab remains architecturally separate from the SIP softphone runtime.

See [`../interview/phase11-questions.md`](../interview/phase11-questions.md). The practical reviewer walkthrough is in [`demo-guide.md`](demo-guide.md).

## Exit criteria

Phase 11 is complete only when all of the following are true:

- the six planned Phase 11 commits are reviewed and their CI checks are green;
- the final Phase 11 aggregate verification passes;
- a reproducible Release package is generated and passes packaged-system acceptance;
- final architecture, operations, portfolio/demo, and evidence documentation are complete;
- no current-facing Phase 11 document still claims `In progress`, `PENDING`, or an unpublished target milestone;
- the phase branch is merged to `main` without discarding the planned commit history;
- the complete Phase 11 verification is rerun successfully on synchronized `main`;
- annotated tag `v1.0.0` is created on that verified `main` commit and pushed.

At Commit 4, these exit criteria are intentionally **not** yet satisfied.

## References

- [`../../README_roadmap.md`](../../README_roadmap.md)
- [`../architecture/system-overview.md`](../architecture/system-overview.md)
- [`../architecture/pjsip-cli.md`](../architecture/pjsip-cli.md)
- [`../architecture/audio-rtp.md`](../architecture/audio-rtp.md)
- [`../architecture/qt-qml.md`](../architecture/qt-qml.md)
- [`../architecture/threading-model.md`](../architecture/threading-model.md)
- [`../architecture/debugging-diagnostics.md`](../architecture/debugging-diagnostics.md)
- [`../architecture/rtsp.md`](../architecture/rtsp.md)
- [`demo-guide.md`](demo-guide.md)
- [`../interview/phase11-questions.md`](../interview/phase11-questions.md)
- [`../../scripts/phase11/verify.sh`](../../scripts/phase11/verify.sh)

## Next phase

There is no planned Phase 12 in the current roadmap. After Phase 11 closure, the repository enters maintenance/release mode at `v1.0.0`; future work should begin from a new explicit roadmap decision rather than being appended silently to the final milestone.
