#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

echo "== clang-format =="
find src tests labs -type f \( -name '*.cpp' -o -name '*.hpp' \) \
    -print0 | xargs -0 clang-format --dry-run --Werror

echo "== debug build + tests =="
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug

echo "== Phase 1 labs =="
./build/debug/labs/cpp/raii/lab_raii
./build/debug/labs/cpp/smart-pointers/lab_smart_pointers
./build/debug/labs/cpp/move-semantics/lab_move_semantics

echo "== sanitizers =="
./scripts/run_sanitizers.sh

echo "== clang-tidy =="
./scripts/run_clang_tidy.sh

echo "Phase 1 verification passed."
