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

for command_name in bash git cmake ctest awk; do
    require_command "${command_name}"
done

[[ -f scripts/phase5/verify.sh ]] || fail "scripts/phase5/verify.sh is missing"
[[ -f scripts/phase6/verify-audio-environment.sh ]] || \
    fail "scripts/phase6/verify-audio-environment.sh is missing"

# Count discovered cases to avoid a vacuous PASS if a focused suite disappears.
run_focused_suite() {
    local description="$1"
    local test_regex="$2"
    local minimum_tests="$3"
    local test_listing
    local test_count

    echo "== ${description} =="
    test_listing="$(ctest --test-dir build/pjsip-debug -N -R "${test_regex}")" || \
        fail "cannot enumerate ${description}"
    test_count="$(printf '%s\n' "${test_listing}" | \
        awk '$1 == "Total" && $2 == "Tests:" { print $3 }')"

    [[ "${test_count}" =~ ^[0-9]+$ ]] || fail "cannot count ${description}"
    (( test_count >= minimum_tests )) || \
        fail "${description}: expected at least ${minimum_tests} tests, found ${test_count}"

    printf 'Discovered tests: %s (minimum %s)\n' "${test_count}" "${minimum_tests}"
    ctest --test-dir build/pjsip-debug \
        -R "${test_regex}" --no-tests=error --output-on-failure
}

echo "== repository scope =="
git diff --check
git diff --cached --check

echo "== Phase 6 audio preflight =="
bash ./scripts/phase6/verify-audio-environment.sh

echo "== inherited Phase 5 quality and regression gate =="
# Reuse the established dependency, format, full-PJSIP, ASan/UBSan,
# clang-tidy, and non-PJSIP checks instead of maintaining a second copy.
bash ./scripts/phase5/verify.sh

run_focused_suite "Phase 6 RTP statistics tests" '^RtpStatisticsTest\.' 4
run_focused_suite "Phase 6 PJSUA2 RTP diagnostics tests" '^Pjsua2RtpDiagnosticsTest\.' 3

echo "Phase 6 automated verification: PASS"
echo "Outgoing/incoming SIP calls, actual two-way audio, and live RTP counters require a separate manual gate."
echo "Do not record SIP credentials, Digest headers, or raw packet captures in committed evidence."
