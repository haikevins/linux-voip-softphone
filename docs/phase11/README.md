# Phase 11 — Final Engineering Release

## Status

**Completed** — release identifier `v1.0.0`.

Phase 11 closes the planned project roadmap by turning the accepted Phase 0–10 work into one reviewable engineering release with a reproducible Release build, packaged executables, final system acceptance, consolidated architecture/operations documentation, portfolio-facing presentation, and sanitized closure evidence. The annotated `v1.0.0` tag is published only after merge and post-merge verification; Git refs remain authoritative for publication state.

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
- final README/demo presentation and release evidence;
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
- `evidence/phase11/` with the closure verification and release-package summaries.

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
5. **Closure verification** — freezes sanitized evidence and current-facing completed status. Merge, synchronized `main` verification, and annotated `v1.0.0` publication remain explicit release operations after this commit.

A green package smoke test proves the documented packaged behavior in the tested environment; it is not a substitute for live SIP credentials, external audio hardware, or arbitrary deployment certification.

## Evidence

The sanitized Phase 11 closure evidence is:

```text
evidence/phase11/
├── README.md
├── automated-verification.txt
└── release-verification.txt
```

The summaries record the aggregate Phase 11 verification contract, Release/TGZ package inventory, packaged CLI/GUI acceptance boundary, and the last pre-closure branch CI observation without retaining build trees, credentials, raw SIP material, or private packet captures.

The closure evidence does not claim that `v1.0.0` is already published. Merge, post-merge verification, and tag publication are separate release operations checked through Git refs.

## Interview focus

Be ready to explain why a final release is more than a successful Debug build; why the packaged artifact must be exercised after extraction; why `v1.0.0` is created only after merge and post-merge verification; how the Qt/application/PJSIP/threading boundaries reduce ownership risk; and why the RTSP mini-lab remains architecturally separate from the SIP softphone runtime.

See [`../interview/phase11-questions.md`](../interview/phase11-questions.md). The practical reviewer walkthrough is in [`demo-guide.md`](demo-guide.md).

## Exit criteria

The Phase 11 repository-scope closure criteria are satisfied when:

- the six planned Phase 11 commits are present and individually verified before progression;
- the complete Phase 11 aggregate verification passes on the closure candidate;
- the reproducible Release package passes packaged-system acceptance;
- final architecture, operations, portfolio/demo, and evidence documentation are complete;
- current-facing Phase 11 documentation is in the completed state;
- the closure commit passes branch CI before merge.

Release publication then requires three explicit operations outside this closure commit: merge the reviewed history to `main`, rerun the complete Phase 11 gate on synchronized `main`, and create/push annotated tag `v1.0.0` on that verified commit. Do not move or pre-create the tag.

## References

- [`../../ROADMAP.md`](../../ROADMAP.md)
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

There is no planned Phase 12 in the current roadmap. After the Phase 11 closure commit, the repository enters release/maintenance mode for `v1.0.0`; future work should begin from a new explicit roadmap decision rather than being appended silently to the final milestone.
