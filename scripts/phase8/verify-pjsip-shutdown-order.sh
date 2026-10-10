#!/usr/bin/env bash
# Phase 8 Commit 9: static shutdown-order regression + canonical PJSIP suite.
# No credentials or network calls are required by the gate itself.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT"

if [[ "$(git branch --show-current)" != 'phase/8-multithreading' ]]; then
    echo 'ABORT: expected phase/8-multithreading' >&2
    exit 1
fi

python3 -B -m unittest discover -s tests/phase8 -p test_pjsip_shutdown_order.py -v
printf 'PASS: Phase 8 PJSIP source shutdown-order contract\n'

if [[ "${PHASE8_PJSIP_STATIC_ONLY:-0}" == '1' ]]; then
    echo 'STATIC ONLY: PJSIP compilation and dynamic CTests NOT RUN.'
    exit 0
fi
if [[ "${PHASE8_PJSIP_STATIC_ONLY:-0}" != '0' ]]; then
    echo 'ABORT: PHASE8_PJSIP_STATIC_ONLY must be 0 or 1' >&2
    exit 1
fi

BUILD_DIR="${PHASE8_PJSIP_BUILD_DIR:-$ROOT/build/qt-pjsip-gui-debug}"
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "ABORT: canonical build missing: $BUILD_DIR" >&2
    exit 1
fi
if ! grep -Fxq 'ENABLE_PJSIP:BOOL=ON' "$BUILD_DIR/CMakeCache.txt"; then
    echo 'ABORT: PJSIP support must be ON in canonical CMake cache' >&2
    exit 1
fi

cmake --build "$BUILD_DIR" --target voip_pjsip pjsip_config_tests -j"$(nproc)"
ctest --test-dir "$BUILD_DIR" --output-on-failure -L '^pjsip$'
echo 'PASS: Phase 8 PJSIP shutdown-order build and CTest gate'
