# Phase 11 Evidence — Final Engineering Release

## Status

Completed. Phase 11 established the final engineering-release workflow and the `v1.0.0` milestone.

The closure commit records the completed repository scope. Publication of the annotated `v1.0.0` tag remains a separate release operation after merge and post-merge verification on synchronized `main`; Git refs are authoritative for publication state.

## Verification scope

Evidence covers the inherited Phase 10 regression/quality chain, Phase 11 release-baseline checks, the canonical Release + Qt/PJSIP build, install staging, CPack TGZ generation, expected executable inventory, extracted-package path safety, runtime dependency resolution, packaged CLI smoke, bounded offscreen Qt GUI startup, documentation/evidence structure, and branch CI progression.

## Environment

The documented environment is Linux with CMake/Ninja, Qt 6/QML, GoogleTest, clang tooling, and PJPROJECT/PJSIP 2.17 exposed through `PKG_CONFIG_PATH`. The Release package is a Linux binary artifact that uses host runtime libraries; it is not claimed to be a self-contained cross-distribution bundle.

## Automated verification

Run the complete closure-candidate gate with:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase11/verify.sh
```

The closure commit is committed only after this command passes locally. The last completed pre-closure documentation commit (Commit 5) also completed all 4/4 branch CI jobs successfully. Closure-commit CI must pass again before merge.

See `automated-verification.txt` for the concise gate summary.

## Manual verification

Phase 11 does not manufacture a new live SIP/audio PASS. Live registration, call, RTP/audio, and human listening evidence remains owned by the earlier phases and their recorded environments. The Phase 11-specific runtime check is the deterministic packaged-system acceptance, including a bounded offscreen Qt startup smoke that does not require live credentials.

## Artifacts

- `automated-verification.txt` — aggregate closure verification and CI boundary summary.
- `release-verification.txt` — Release/package inventory and extracted-package acceptance summary.

Generated build trees, TGZ binaries, raw logs, credentials, and packet captures are not committed as evidence.

## Results

The documented Phase 11 release path builds the final Release configuration, stages installation, creates the expected TGZ package, verifies the packaged executable inventory, extracts the artifact safely, checks runtime dependency resolution, exercises the packaged CLI entry points, and performs bounded GUI startup. The aggregate verifier also preserves the inherited regression, static-analysis, sanitizer, RTSP, and evidence/documentation contracts.

These results establish the engineering-release workflow for the tested Linux environment. They do not claim universal distribution portability, production SIP-provider certification, or arbitrary hardware/network compatibility.

## Security and redaction

Do not commit SIP passwords, Digest authorization material, SRTP keying material, private packet captures, raw audio/media, core dumps, host credentials, or generated release build trees. Phase 11 evidence retains only concise sanitized outcomes and reproducible commands.

## Reproduction

Run the complete Phase 11 gate:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase11/verify.sh
```

Run release/package checks directly with:

```bash
./scripts/phase11/verify-package.sh
./scripts/phase11/verify-final-acceptance.sh
```

After merge, rerun `./scripts/phase11/verify.sh` on synchronized `main` before creating and pushing annotated tag `v1.0.0`.

## References

- `docs/phase11/README.md`
- `docs/architecture/system-overview.md`
- `docs/phase11/demo-guide.md`
- `docs/interview/phase11-questions.md`
- `scripts/phase11/verify.sh`
- `scripts/phase11/verify-package.sh`
- `scripts/phase11/verify-final-acceptance.sh`
- `ROADMAP.md`
