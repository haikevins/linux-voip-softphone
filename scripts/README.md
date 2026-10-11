# Script Taxonomy

`scripts/` contains executable repository workflows. Keep phase wrappers small; tests and CMake registration belong under their normal repository taxonomy.

## Layout

```text
scripts/
├── setup/      environment/bootstrap helpers
├── dev/        common build/test entry points
├── quality/    repository-wide quality gates
├── phase0/ ... phase6/
└── phase8/     small Phase 8 reproduction/verification entry points
```

There is no `scripts/phase7/`; Phase 7 uses the normal Qt/PJSIP CMake/CTest presets plus inherited verification.

## Phase 8 entry points

- `verify.sh` — aggregate offline verification using canonical CMake/CTest plus the retained shutdown boundary checks.
- `verify-threading.sh [core|stress|tsan|all]` — concurrency core, repeated MPMC stress, and root-project TSan reproduction.
- `run-pjsip-live-lifecycle-probe.sh` — explicit opt-in live SIP lifecycle probe (`PHASE8_LIVE_SIP=1`).

Source-contract helper modules live with integration tests under `tests/integration/support/`; they are test logic rather than operator scripts.

## Conventions

- Shell entry points use Bash and `set -euo pipefail`.
- Scripts resolve the repository root relative to their location.
- Verification must not silently generate committed evidence.
- Python verification must not leave `.pyc` files in the checkout.
- Live SIP is never hidden inside an offline aggregate.
- Offline/synthetic PASS does not prove universal PJSUA2 callback quiescence or production shutdown safety.
- Never commit SIP credentials, Digest response material, SRTP inline keys, or raw sensitive captures.
- Prefer repository-wide CMake/CTest/quality infrastructure over phase-specific build systems.

## Common commands

```bash
./scripts/dev/build.sh
./scripts/dev/test.sh
./scripts/quality/verify-docs-structure.sh
./scripts/quality/verify-evidence-structure.sh
./scripts/phase8/verify-threading.sh core
./scripts/phase8/verify.sh
```

Phase-specific prerequisites and boundaries remain authoritative under `docs/phaseN/`.
