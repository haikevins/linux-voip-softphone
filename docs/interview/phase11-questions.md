# Phase 11 Interview Review — Final Engineering Release

Use the repository implementation, documentation, tests, release scripts, package artifact, and evidence as the basis for the answers. Explain the engineering decision and the observed behavior; do not memorize isolated one-line definitions.

## Review scope

- final Qt/QML → application → threaded SIP/PJSUA2 → RTP/audio architecture;
- ownership, callback, and shutdown boundaries retained from the threading phase;
- separation between the SIP softphone and RTSP/SDP/RTP mini-lab;
- reproducible CMake Release build, install rules, and CPack TGZ packaging;
- source-level regression versus package-level system acceptance;
- CI, post-merge verification, evidence provenance, and `v1.0.0` release discipline;
- limits of what the final engineering release does and does not claim.

## Questions and answers

### Final architecture

#### Why does the final release keep QML away from direct PJSUA2 ownership?

PJSUA2 endpoint, account, call, callback, and media lifetimes are backend concerns. Keeping them behind the application/threaded engine boundary prevents UI object lifetime and Qt event-loop timing from becoming the lifetime policy for telephony objects. QML consumes state and emits user intent; it does not own the SIP stack.

#### Why is the application layer separate from both Qt and PJSUA2?

It gives call state and validation one place that can be tested without a GUI or a live SIP stack. Qt becomes presentation, while PJSUA2 becomes an implementation of the SIP engine boundary. That separation was important for unit tests, asynchronous event handling, and later worker-thread integration.

#### What is the final production call path?

Conceptually: Qt user action → application controller → serialized SIP backend → PJSUA2 signaling → call/media callbacks → Linux audio/RTP state → queued application/UI update. Each arrow is a boundary where ownership, thread context, or protocol state changes.

#### Why is RTSP not merged into the SIP call path in Phase 11?

Phase 10 validated RTSP/SDP/RTP as a focused protocol mini-lab. The SIP softphone already has its own PJSUA2/RTP/audio runtime. Combining them in the final release would add a new product feature and new lifecycle/media risks after the RTSP milestone had already been accepted. Phase 11 packages both but preserves their boundaries.

### Release engineering

#### Why is a successful Debug build insufficient for `v1.0.0`?

A Debug build proves only that one developer-oriented configuration compiled and tested. The release milestone must also prove the intended Release configuration, install rules, packaged executable inventory, runtime dependencies, and extracted-artifact smoke behavior.

#### What does reproducible packaging mean in this repository?

Given the documented toolchain/dependencies and the same source revision, the repository has an explicit `release` CMake preset and scripted path that rebuilds the release targets, stages installation, and creates a predictably named TGZ artifact. It does not mean bit-for-bit reproducibility across arbitrary compilers or Linux distributions unless such a stronger guarantee is separately demonstrated.

#### Why test binaries after extracting the TGZ package?

Because build-tree tests cannot detect every packaging defect. An install rule could omit an executable, a packaged binary could have an unresolved runtime dependency, or the packaged layout could differ from the build tree. Extracting and exercising the package validates the actual deliverable.

#### Why is the GUI smoke test bounded and offscreen?

CI and automated acceptance cannot depend on a human desktop session. Offscreen startup proves that the packaged Qt application can initialize in a bounded non-interactive environment. A timeout exit can therefore be an accepted smoke outcome when the process remains healthy until the harness terminates it. This does not replace manual UI usability or live SIP/audio checks.

### Verification and evidence

#### Why does Phase 11 inherit the Phase 10 verifier instead of replacing it with a new short test list?

The final release must preserve the accumulated regression guarantees from earlier milestones. The Phase 11 scripts add release-specific checks on top of the inherited chain; they do not redefine older acceptance by testing only the final package.

#### What is the difference between source regression and packaged-system acceptance?

Source regression checks implementation behavior across unit/integration tests, static analysis, sanitizers, stress, and phase-specific gates. Packaged-system acceptance checks the installed deliverable: archive safety, executable inventory, dependencies, CLI behavior, and GUI startup. Both are necessary because they catch different classes of failure.

#### Does a green package smoke test prove production SIP/audio behavior?

No. A help/smoke run can prove process startup and basic packaged behavior without credentials or external infrastructure. Live registration/call behavior and audible two-way audio are separate evidence layers from earlier phases and remain scoped to the environments in which they were observed.

#### Why must evidence avoid raw SIP credentials and packet captures?

Release evidence should be reviewable without leaking secrets or unrelated identifying data. Sanitized summaries, selected counters, commands, revision IDs, and PASS/FAIL outcomes are sufficient for the engineering claim. Passwords, Digest authorization data, keying material, and raw captures are not appropriate repository artifacts.

### Git and release discipline

#### Why is `v1.0.0` not created on the Phase 11 branch?

The branch is still an integration candidate. The final tag should identify the exact `main` commit that includes the merged phase and has passed post-merge verification. Tagging earlier would make the release identifier point at a state that was not the final integrated release.

#### Why preserve the six Phase 11 commits instead of squashing them into one release commit?

Each commit has a distinct engineering responsibility: scaffold, release build/package, final acceptance, architecture/operations docs, portfolio/demo docs, and closure/evidence. Preserving that history makes review and regression attribution clearer and matches the phase workflow used throughout the project.

#### What should happen if the closure commit passes CI but post-merge verification fails?

Do not create or move `v1.0.0`. Fix the issue through a reviewed change, rerun CI, merge the correction, and repeat post-merge verification. A release tag should record an accepted state, not be moved later to hide a failed release candidate.

### Scope and engineering judgment

#### Why not add more user-facing features before calling the project 1.0?

The project roadmap defines 1.0 as the final engineering release of the demonstrated learning/portfolio scope, not as feature parity with commercial softphones. Adding unrelated features at closure would increase risk and blur the evidence for already accepted architecture. New capability should start with a new explicit requirement and roadmap.

#### What is the strongest engineering claim the final repository can make?

It demonstrates a phase-driven C++ Linux softphone project with explicit build/test/CI discipline; SIP/PJSIP signaling; RTP/audio integration; Qt/QML presentation; threaded ownership and teardown reasoning; diagnostics; an RTSP protocol mini-lab; and a scripted release/package acceptance path. Claims remain limited to the documented source revisions, tests, lab environments, and evidence rather than universal production certification.

#### If an interviewer asks for one place to understand the whole project, where should you point first?

Start with the root README after Commit 5 for the portfolio-facing overview, then `docs/architecture/system-overview.md` for the technical map, `docs/phase11/README.md` for release/verification workflow, and the phase-specific architecture/evidence documents for deeper proof.

## Phase boundary

Phase 11 repository scope is complete at the closure commit: release build/package mechanics, packaged-system acceptance, final architecture/operations documentation, portfolio/demo guidance, and sanitized evidence are all in place. The annotated `v1.0.0` tag is still a separate release operation after merge and post-merge verification.

The release must not be described as universally production-certified. Live SIP/audio observations, hardware/network behavior, and distribution portability remain limited to their documented evidence scopes.

## References

- `README.md`
- `ROADMAP.md`
- `docs/phase11/README.md`
- `docs/architecture/system-overview.md`
- `docs/phase11/demo-guide.md`
- `scripts/phase11/verify.sh`
- `scripts/phase11/verify-package.sh`
- `scripts/phase11/verify-final-acceptance.sh`
- `evidence/phase11/README.md`

