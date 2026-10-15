#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${ROOT_DIR}"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

require_command() {
    local command_name="$1"
    command -v "${command_name}" >/dev/null 2>&1 || \
        fail "required command not found: ${command_name}"
}

for command_name in bash git cmake ctest awk pkg-config timeout; do
    require_command "${command_name}"
done

[[ -f scripts/phase6/verify.sh ]] || fail "scripts/phase6/verify.sh is missing"
[[ -x scripts/phase7/verify-gui-smoke.sh ]] || \
    fail "scripts/phase7/verify-gui-smoke.sh is missing or not executable"

if ! pkg-config --exists 'libpjproject >= 2.17'; then
    fail "libpjproject >= 2.17 is not visible to pkg-config; export PKG_CONFIG_PATH for the Phase 5 local PJPROJECT installation"
fi

run_phase7_suite() {
    local build_dir="$1"
    local description="$2"
    local minimum_tests=11
    local listing
    local count

    echo "== ${description} =="
    listing="$(ctest --test-dir "${build_dir}" -N -L phase7)" || \
        fail "cannot enumerate ${description}"
    count="$(printf '%s\n' "${listing}" | \
        awk '$1 == "Total" && $2 == "Tests:" { print $3 }')"

    [[ "${count}" =~ ^[0-9]+$ ]] || fail "cannot count ${description}"
    (( count >= minimum_tests )) || \
        fail "${description}: expected at least ${minimum_tests} tests, found ${count}"

    printf 'Discovered tests: %s (minimum %s)\n' "${count}" "${minimum_tests}"
    ctest --test-dir "${build_dir}" -L phase7 --no-tests=error --output-on-failure
}

echo "== repository scope =="
git diff --check
git diff --cached --check

echo "== inherited Phase 6 quality and regression gate =="
bash ./scripts/phase6/verify.sh

echo "== Qt-only configure and build =="
cmake --preset qt-debug
cmake --build --preset qt-debug -j

run_phase7_suite build/qt-debug "Phase 7 Qt application tests"

echo "== Qt-only full regression =="
ctest --test-dir build/qt-debug --output-on-failure

echo "== Qt + PJSIP configure and build =="
cmake --preset qt-pjsip-debug
cmake --build --preset qt-pjsip-debug -j

run_phase7_suite build/qt-pjsip-debug "Phase 7 Qt + PJSIP integration tests"

echo "== documentation and evidence structure =="
bash ./scripts/quality/verify-docs-structure.sh
bash ./scripts/quality/verify-evidence-structure.sh

echo "Phase 7 automated verification: PASS"
echo "Live SIP registration, outgoing/incoming calls, two-way audio, and visible GUI state transitions remain a separate manual gate."
echo "Do not record SIP credentials, Digest headers, SRTP keys, or raw packet captures in committed evidence."
