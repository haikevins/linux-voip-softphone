#!/usr/bin/env bash
# Deterministic polling-policy test plus canonical PJSIP link/CTest coverage.
# This does not inject SIP callbacks or establish safety after a timeout.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT"

if [[ "$(git branch --show-current)" != 'phase/8-multithreading' ]]; then
    echo 'ABORT: expected phase/8-multithreading' >&2
    exit 1
fi

BUILD_DIR="${PHASE8_PJSIP_BUILD_DIR:-$ROOT/build/qt-pjsip-gui-debug}"
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "ABORT: missing canonical CMakeCache.txt: $BUILD_DIR" >&2
    exit 1
fi
if ! grep -Fxq 'ENABLE_PJSIP:BOOL=ON' "$BUILD_DIR/CMakeCache.txt"; then
    echo 'ABORT: canonical build must enable PJSIP' >&2
    exit 1
fi

# Reconfigure to register the new pure-C++ CTest within the canonical suite.
cmake -S . -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target \
    voip_pjsip pjsip_config_tests phase8_pjsip_call_slot_cleanup_tests \
    -j"$(nproc)"

ctest --test-dir "$BUILD_DIR" --output-on-failure \
    -R '^Phase8PjsipCallSlotCleanupTest$'
ctest --test-dir "$BUILD_DIR" --output-on-failure -L '^pjsip$'

echo 'PASS: Phase 8 PJSIP polling policy and canonical PJSIP gate'
echo 'NOTE: no live SIP callback or timeout-destruction safety verified'
