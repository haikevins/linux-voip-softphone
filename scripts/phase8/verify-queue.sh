#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SOURCE_DIR="$REPO_ROOT/tests/phase8"
BUILD_DIR="${PHASE8_BUILD_DIR:-$REPO_ROOT/build/phase8-queue-debug}"

command -v cmake >/dev/null || { echo 'ERROR: cmake is required' >&2; exit 1; }
command -v ctest >/dev/null || { echo 'ERROR: ctest is required' >&2; exit 1; }
test -f "$SOURCE_DIR/CMakeLists.txt"
test -f "$REPO_ROOT/src/concurrency/ThreadSafeQueue.hpp"
test -f "$REPO_ROOT/src/concurrency/SerialTaskWorker.hpp"
test -f "$SOURCE_DIR/test_thread_ownership.cpp"
test -f "$SOURCE_DIR/test_mpmc_stress.cpp"

printf 'Phase 8 queue, worker, ownership and MPMC stress verification\nSource: %s\nBuild:  %s\n' "$SOURCE_DIR" "$BUILD_DIR"
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD_DIR" --parallel 2
ctest --test-dir "$BUILD_DIR" --output-on-failure
