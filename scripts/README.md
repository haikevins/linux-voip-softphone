# Script Taxonomy

`scripts/` contains executable repository workflows. Directory names define responsibility; individual filenames stay focused on one reusable task.

## Layout

```text
scripts/
├── setup/      environment/bootstrap helpers
├── dev/        common build/test entry points
├── quality/    repository-wide quality gates
├── phase0/ ... phase6/
└── phase8/     Phase 8 verification and audit workflows
```

There is no `scripts/phase7/`. Phase 7 used Qt/PJSIP CMake/CTest presets, repository-wide quality checks, and inherited Phase 5/6 regression gates rather than a dedicated phase wrapper.

Phase 8 consolidated its incremental hardening scripts by responsibility; exact historical versions remain available through Git history.

## Phase 8 entry points

- `verify.sh` — aggregate automated offline gate.
- `verify-static-analysis.sh [format|tidy|all]` — local static-analysis reproduction.
- `verify-threading.sh [queue|stress|tsan|all]` — queue/worker, MPMC stress, and standalone TSan verification.
- `verify-pjsip.sh <mode>` — offline Qt/PJSIP lifetime/shutdown/source-audit gates and recorded live-evidence checks.
- `verify-final-runtime-acceptance.sh [--automatic|--manual]` — offline runtime gate plus separate human GUI stage.
- `verify-roadmap-exit.sh` — roadmap/source/evidence boundary audit.
- `run-pjsip-live-lifecycle-probe.sh` — explicit opt-in live SIP lifecycle probe (`PHASE8_LIVE_SIP=1`).
- `audit-*.py` and `gui-runtime-probe.py` — reusable analysis/runtime helpers used by verification.

## Conventions

- Shell entry points use Bash and `set -euo pipefail`.
- Scripts resolve the repository root relative to their location.
- Verification must not silently generate committed evidence.
- Python verification must not leave `.pyc` files in the checkout.
- Live SIP is never hidden inside an offline aggregate.
- Offline/synthetic PASS does not prove universal PJSUA2 callback quiescence or production shutdown safety.
- Never commit SIP credentials, Digest response material, SRTP inline keys, or raw sensitive captures.
- Prefer extending an existing consolidated verifier over adding one wrapper per bug/commit.

## Common commands

```bash
./scripts/dev/build.sh
./scripts/dev/test.sh
./scripts/quality/verify-docs-structure.sh
./scripts/quality/verify-evidence-structure.sh
./scripts/phase8/verify-static-analysis.sh all
./scripts/phase8/verify-threading.sh queue
./scripts/phase8/verify-pjsip.sh shutdown-order
./scripts/phase8/verify.sh
```

Phase-specific prerequisites and workflow details remain authoritative under `docs/phaseN/`.
