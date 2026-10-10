#!/usr/bin/env bash
set -euo pipefail

# A deterministic failure-path MODEL gate + the current production source
# closure inventory. Neither part establishes live PJSUA2 callback-target
# quiescence or actual PJ_EBUSY recovery.
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
FOCUSED="${PHASE8_BUILD_DIR:-$ROOT/build/phase8-queue-debug}"
CANONICAL="${PHASE8_CANONICAL_BUILD_DIR:-$ROOT/build/qt-pjsip-gui-debug}"

for tool in python3 cmake ctest; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'ABORT: required tool unavailable: %s\n' "$tool" >&2
        exit 1
    fi
done

if [[ "$FOCUSED" == "$CANONICAL" ]]; then
    printf 'ABORT: focused and canonical build directories must differ\n' >&2
    exit 1
fi

export PYTHONDONTWRITEBYTECODE=1
cd "$ROOT"

echo '===== BOUNDED PRODUCTION SOURCE RISK INVENTORY ====='
python3 -B - <<'PY'
import importlib.util
from pathlib import Path
import sys

root = Path.cwd()
audit_path = root / 'scripts/phase8/audit-pjsip-production-lifetime.py'
spec = importlib.util.spec_from_file_location('phase8_shutdown_inventory', audit_path)
if spec is None or spec.loader is None:
    raise SystemExit('ABORT: cannot load existing production source audit')
audit = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = audit
spec.loader.exec_module(audit)

outgoing = (root / 'src/pjsip/SipOutgoingCallClient.cpp').read_text(encoding='utf-8')
incoming = (root / 'src/pjsip/SipIncomingCallClient.cpp').read_text(encoding='utf-8')
result = audit.audit_pair(outgoing, incoming)
if result['source_contract'] != 'PASS':
    raise SystemExit('ABORT: production source structure changed; re-audit before proceeding')

for client, state in result['clients'].items():
    if state['risks']:
        raise SystemExit(
            f'ABORT: {client} source closure risk inventory is not empty: '
            f'{sorted(state["risks"])}'
        )
    print(f'{client}: 0 known source closure risks OPEN')

print('PASS: production source closure inventory (offline structural contract)')
print('Live PJSUA2 callback-target quiescence: NOT TESTED')
print('Actual PJ_EBUSY recovery: NOT TESTED')
PY

echo '===== FOCUSED MODEL TARGET ====='
cmake -S tests/phase8 -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$FOCUSED" --target phase8_pjsip_shutdown_failure_paths_tests -j 2
ctest --test-dir "$FOCUSED" --output-on-failure \
    -R '^Phase8PjsipShutdownFailurePathsTest$'

echo '===== CANONICAL MODEL TARGET ====='
if [[ ! -f "$CANONICAL/CMakeCache.txt" ]]; then
    printf 'ABORT: canonical build not configured: %s\n' "$CANONICAL" >&2
    exit 1
fi
cmake -S . -B "$CANONICAL"
cmake --build "$CANONICAL" --target phase8_pjsip_shutdown_failure_paths_tests -j 2
ctest --test-dir "$CANONICAL" --output-on-failure \
    -R '^Phase8PjsipShutdownFailurePathsTest$'

printf '%s\n' 'PASS: Phase 8 PJSIP shutdown failure-path MODEL gate (live PJSUA2 NOT TESTED)'
