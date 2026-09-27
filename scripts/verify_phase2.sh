#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

echo "== clang-format =="
find src tests labs -type f \( -name '*.cpp' -o -name '*.hpp' \) \
    -print0 | xargs -0 clang-format --dry-run --Werror

echo "== debug build + full test suite =="
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug --output-on-failure

echo "== Phase 2 process labs =="
./build/debug/labs/linux/process-lifecycle/lab_process_lifecycle --self-test
./build/debug/labs/linux/exec-child/lab_exec_child --self-test
./build/debug/labs/linux/signal-handling/lab_signal_handling --self-test

echo "== Phase 2 IPC integration =="
ctest --preset debug \
    -R UnixDomainSocketIpcIntegrationTest \
    --output-on-failure

echo "== sanitizers (clean rebuild) =="
cmake --preset sanitizers
cmake --build --preset sanitizers --clean-first -j
ctest --preset sanitizers --output-on-failure

echo "== clang-tidy (clean rebuild) =="
./scripts/run_clang_tidy.sh

echo "Phase 2 verification passed."
