#!/usr/bin/env bash
# Focused Qt ownership/lifetime regression for the source-reviewed Phase 8 slice.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT"

if [[ "$(git branch --show-current)" != "phase/8-multithreading" ]]; then
    echo "ABORT: expected phase/8-multithreading" >&2
    exit 1
fi

BUILD_DIR="${PHASE8_QT_BUILD_DIR:-$ROOT/build/qt-pjsip-gui-debug}"
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "ABORT: canonical Qt build directory not configured: $BUILD_DIR" >&2
    exit 1
fi

if ! grep -Eq '^ENABLE_QT_GUI:BOOL=ON$' "$BUILD_DIR/CMakeCache.txt"; then
    echo "ABORT: selected build must enable Qt GUI" >&2
    exit 1
fi

cmake --build "$BUILD_DIR" --target qt_registration_backend_tests -j"$(nproc)"
ctest --test-dir "$BUILD_DIR" --output-on-failure -R '^QtRegistrationBackendTest$'
echo "PASS: Qt owner-thread and shutdown regression gate"
