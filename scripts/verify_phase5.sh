#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

require_command() {
    local command_name="$1"
    command -v "${command_name}" >/dev/null 2>&1 || fail "required command not found: ${command_name}"
}

require_command cmake
require_command ctest
require_command clang-format
require_command pkg-config

[[ -f scripts/verify_pjsip_dependency.sh ]] || \
    fail "scripts/verify_pjsip_dependency.sh is missing"

echo "== repository scope =="
git diff --check

echo "== clang-format =="
find src tests labs -type f \( -name '*.cpp' -o -name '*.hpp' \) \
    -print0 | xargs -0 clang-format --dry-run --Werror

echo "== PJSIP dependency =="
bash ./scripts/verify_pjsip_dependency.sh

echo "== PJSIP debug build =="
cmake --preset pjsip-debug
cmake --build --preset pjsip-debug -j

echo "== Phase 5 PJSIP tests =="
ctest --test-dir build/pjsip-debug -L pjsip --output-on-failure

echo "== PJSIP full regression =="
ctest --test-dir build/pjsip-debug --output-on-failure

echo "== PJSIP sanitizers =="
rm -rf build/sanitizers
cmake --preset sanitizers -DENABLE_PJSIP=ON
cmake --build --preset sanitizers -j
ctest --preset sanitizers --output-on-failure

echo "== PJSIP clang-tidy =="
rm -rf build/clang-tidy
cmake --preset clang-tidy -DENABLE_PJSIP=ON
cmake --build --preset clang-tidy -j

echo "== non-PJSIP regression =="
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug --output-on-failure

echo "Phase 5 automated verification: PASS"
echo "Live Asterisk registration and call-flow checks remain manual and must not record SIP secrets."
