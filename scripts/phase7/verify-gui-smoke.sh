#!/usr/bin/env bash
set -euo pipefail

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

command -v timeout >/dev/null 2>&1 || fail "required command not found: timeout"

binary="${1:-}"
[[ -n "${binary}" ]] || fail "usage: $0 <gui-binary>"
[[ -x "${binary}" ]] || fail "GUI binary is missing or not executable: ${binary}"

log_file="$(mktemp)"
trap 'rm -f "${log_file}"' EXIT

set +e
env \
    QT_QPA_PLATFORM=offscreen \
    QT_QUICK_BACKEND=software \
    timeout --signal=TERM 3s "${binary}" >"${log_file}" 2>&1
status=$?
set -e

# A healthy GUI remains in the Qt event loop until the smoke-test timeout.
# GNU timeout returns 124 when it terminates a still-running process.
if [[ ${status} -ne 124 ]]; then
    cat "${log_file}" >&2
    fail "GUI exited before the 3-second smoke window (status ${status})"
fi

printf 'GUI launch smoke test: PASS (%s)\n' "${binary}"
