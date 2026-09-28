#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 3 ]]; then
    printf 'Usage: %s REGISTER_BIN CALL_BIN LISTEN_BIN\n' "$0" >&2
    exit 2
fi

REGISTER_BIN="$1"
CALL_BIN="$2"
LISTEN_BIN="$3"
MISSING_PASSWORD_ENV="VOIP_PHASE5_INTEGRATION_MISSING_PASSWORD"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

require_executable() {
    local binary="$1"
    [[ -x "${binary}" ]] || fail "missing executable: ${binary}"
}

require_text() {
    local output="$1"
    local expected="$2"
    grep -Fq -- "${expected}" <<<"${output}" || fail "missing expected text: ${expected}"
}

verify_help() {
    local binary="$1"
    local expected_usage="$2"
    local expected_option="$3"
    local output

    output="$("${binary}" --help 2>&1)" || fail "--help failed for ${binary}"
    require_text "${output}" "${expected_usage}"
    require_text "${output}" "${expected_option}"
    require_text "${output}" "--password-env"
}

verify_missing_password() {
    local binary="$1"
    shift

    local output
    local status

    set +e
    output="$(env -u "${MISSING_PASSWORD_ENV}" \
        "${binary}" "$@" --password-env "${MISSING_PASSWORD_ENV}" 2>&1)"
    status=$?
    set -e

    [[ "${status}" -eq 2 ]] || \
        fail "expected exit code 2 for missing password from ${binary}; got ${status}"
    require_text "${output}" "${MISSING_PASSWORD_ENV}"
    require_text "${output}" "not set or is empty"
}

require_executable "${REGISTER_BIN}"
require_executable "${CALL_BIN}"
require_executable "${LISTEN_BIN}"

verify_help \
    "${REGISTER_BIN}" \
    "Usage: voip-sip-register --username USER --server HOST [options]" \
    "--timeout-ms"

verify_help \
    "${CALL_BIN}" \
    "Usage: voip-sip-call --username USER --server HOST --target USER [options]" \
    "--call-timeout-ms"

verify_help \
    "${LISTEN_BIN}" \
    "Usage: voip-sip-listen --username USER --server HOST [options]" \
    "--incoming-timeout-ms"

verify_missing_password \
    "${REGISTER_BIN}" \
    --username integration-user \
    --server 127.0.0.1 \
    --listen-port 0

verify_missing_password \
    "${CALL_BIN}" \
    --username integration-user \
    --server 127.0.0.1 \
    --listen-port 0 \
    --target integration-peer

verify_missing_password \
    "${LISTEN_BIN}" \
    --username integration-user \
    --server 127.0.0.1 \
    --listen-port 0

printf 'PJSIP CLI contract integration: PASS\n'
