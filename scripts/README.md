# Script Taxonomy

The `scripts/` tree contains executable repository workflows. Directories define responsibility so filenames do not need to repeat repository-wide or phase context.

## Layout

```text
scripts/
├── setup/
├── dev/
├── quality/
├── phase0/ ... phase6/
└── phase8/
    ├── verify.sh
    ├── verify-static-analysis.sh
    ├── verify-threading.sh
    ├── verify-pjsip.sh
    ├── verify-final-runtime-acceptance.sh
    ├── verify-roadmap-exit.sh
    ├── run-pjsip-live-lifecycle-probe.sh
    ├── audit-pjsip-production-lifetime.py
    ├── audit-pjsip-shutdown-integration.py
    ├── audit-qt-pjsip-ownership.py
    └── gui-runtime-probe.py
```

There is no `scripts/phase7/` directory. Phase 7 reused repository-wide quality,
Qt/PJSIP CMake/CTest presets and inherited regression gates rather than adding a
redundant phase wrapper.

Phase 8 originally accumulated one wrapper per incremental hardening slice. The
current tree consolidates those wrappers by responsibility while retaining the
exact historical scripts in Git history.

## Phase 8 responsibilities

- `verify.sh` — aggregate automated **offline** gate. It validates docs/evidence
  structure, script/Python syntax, the recorded live-evidence summary, final
  offline runtime acceptance and the roadmap/source boundary audit.
- `verify-static-analysis.sh [format|tidy|all]` — local reproduction of the
  GitHub Static Analysis formatting and clang-tidy gates.
- `verify-threading.sh [queue|stress|tsan|all]` — focused queue/worker suite,
  repeated 100,000-event MPMC stress and isolated ThreadSanitizer verification.
- `verify-pjsip.sh <mode>` — consolidated offline Qt/PJSIP lifetime, shutdown,
  source-audit, model and recorded-live-evidence gates. `all-offline` never
  performs a real SIP call.
- `verify-final-runtime-acceptance.sh [--automatic|--manual]` — canonical/offline
  final runtime gate and the separate human graphical GUI stage.
- `verify-roadmap-exit.sh` — read-only roadmap/source/evidence inventory that
  must preserve the production integration `BLOCKED/UNPROVEN` boundary.
- `run-pjsip-live-lifecycle-probe.sh` — explicit opt-in real-SIP experiment
  (`PHASE8_LIVE_SIP=1`), deliberately excluded from all offline aggregates.
- `audit-*.py` — reusable source-structure analyzers; they remain separate
  because Python tests import/validate them directly.
- `gui-runtime-probe.py` — runtime helper used by final GUI acceptance.

## Conventions

- Shell entry points use Bash and `set -euo pipefail`.
- Scripts resolve the repository root relative to their own location.
- Verification does not silently generate committed evidence.
- Python syntax checks use in-memory `compile(...)`; repository verification
  must not create `.pyc` files in the source checkout.
- Live SIP is never hidden inside an offline aggregate.
- A PASS from an offline/synthetic gate is not proof of universal PJSUA2
  callback quiescence, shutdown safety, leak freedom or media behavior.
- SIP credentials, Digest Authorization material, SRTP inline keys and raw
  sensitive captures must never be committed.
- Do not create a new Phase 8 wrapper for each bug or commit. Add a subcommand to
  the appropriate consolidated verifier unless a genuinely independent,
  reusable responsibility requires a new file.

## Common entry points

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

Phase-specific documentation under `docs/phaseN/` remains the authoritative
source for each phase workflow and prerequisites.
