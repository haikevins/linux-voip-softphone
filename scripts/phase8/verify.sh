#!/usr/bin/env bash
# Aggregate Phase 8 automated offline verification.
# This script does NOT prove live SIP/PJSUA2 shutdown safety.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

require_command() {
    local command_name="$1"
    command -v "$command_name" >/dev/null 2>&1 || \
        fail "required command not found: $command_name"
}

for command_name in bash git python3; do
    require_command "$command_name"
done

[[ "$(git branch --show-current)" == 'phase/8-multithreading' ]] || \
    fail 'expected phase/8-multithreading'

required_paths=(
    scripts/quality/verify-docs-structure.sh
    scripts/quality/verify-evidence-structure.sh
    scripts/phase8/verify-final-runtime-acceptance.sh
    scripts/phase8/verify-roadmap-exit.sh
)
for path in "${required_paths[@]}"; do
    [[ -f "$path" ]] || fail "required verification entry point missing: $path"
done

echo '== repository scope =='
git diff --check
git diff --cached --check

echo '== documentation structure =='
bash scripts/quality/verify-docs-structure.sh

echo '== evidence structure =='
bash scripts/quality/verify-evidence-structure.sh

echo '== Phase 8 shell syntax =='
while IFS= read -r -d '' script; do
    bash -n "$script"
done < <(find scripts/phase8 -maxdepth 1 -type f -name '*.sh' -print0 | sort -z)
echo 'Phase 8 shell syntax: PASS'

echo '== Phase 8 Python syntax =='
python3 -B - <<'PY'
from pathlib import Path

paths = sorted(Path('scripts/phase8').glob('*.py'))
paths += sorted(Path('tests/phase8').glob('*.py'))
if not paths:
    raise SystemExit('ERROR: no Phase 8 Python files found')
for path in paths:
    compile(path.read_text(encoding='utf-8'), str(path), 'exec')
print(f'Phase 8 Python syntax: PASS ({len(paths)} file(s))')
PY

echo '== Phase 8 final automated offline runtime acceptance =='
bash scripts/phase8/verify-final-runtime-acceptance.sh --automatic

echo '== Phase 8 roadmap/source boundary audit =='
bash scripts/phase8/verify-roadmap-exit.sh

echo 'Phase 8 automated offline verification: PASS'
echo 'BOUNDARY: live SIP/PJSUA2 shutdown safety remains BLOCKED/UNPROVEN.'
echo 'BOUNDARY: manual graphical GUI acceptance remains a separate explicit gate.'
