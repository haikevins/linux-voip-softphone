#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CANONICAL="${PHASE8_CANONICAL_BUILD_DIR:-$ROOT/build/qt-pjsip-gui-debug}"
FOCUSED="${PHASE8_BUILD_DIR:-$ROOT/build/phase8-queue-debug}"

for tool in python3 cmake ctest; do
    command -v "$tool" >/dev/null || { echo "ABORT: required command unavailable: $tool" >&2; exit 1; }
done

cd "$ROOT"
echo '===== SOURCE AUDIT (REAL PRODUCTION C++ FILES) ====='
python3 -B scripts/phase8/audit-pjsip-production-lifetime.py

echo '===== NINE MUTATION-ORIENTED REGRESSIONS ====='
python3 -B tests/phase8/test_pjsip_production_lifetime_audit.py

echo '===== FOCUSED CTEST REGISTRATION ====='
cmake -S tests/phase8 -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
ctest --test-dir "$FOCUSED" --output-on-failure -R '^Phase8PjsipProductionAuditTest$'

echo '===== CANONICAL CTEST REGISTRATION ====='
cmake -S . -B "$CANONICAL"
ctest --test-dir "$CANONICAL" --output-on-failure -R '^Phase8PjsipProductionAuditTest$'

printf '%s\n' 'PASS: Phase 8 production callback source audit (static only; live quiescence INCONCLUSIVE)'
