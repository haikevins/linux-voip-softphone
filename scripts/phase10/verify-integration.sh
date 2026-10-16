#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${ROOT_DIR}"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

for command_name in cmake ctest awk nproc; do
    command -v "${command_name}" >/dev/null 2>&1 || \
        fail "required command not found: ${command_name}"
done

cmake --preset debug
cmake --build --preset debug --target phase10_rtsp_end_to_end_tests -j"$(nproc)"

listing="$(ctest --test-dir build/debug -N -L phase10-integration)" || \
    fail "cannot enumerate Phase 10 integration tests"
count="$(printf '%s\n' "${listing}" | awk '$1 == "Total" && $2 == "Tests:" { print $3 }')"

[[ "${count}" =~ ^[0-9]+$ ]] || fail "cannot count Phase 10 integration tests"
(( count == 3 )) || fail "expected 3 Phase 10 integration tests, found ${count}"

printf 'Phase 10 integration tests discovered: %s\n' "${count}"
ctest --test-dir build/debug \
    -L phase10-integration \
    --repeat until-fail:3 \
    --no-tests=error \
    --output-on-failure

printf 'Phase 10 loopback integration verification: PASS\n'
