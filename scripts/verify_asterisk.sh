#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PJSIP_SAMPLE="${ROOT_DIR}/config/asterisk/pjsip.conf.example"
DIALPLAN_SAMPLE="${ROOT_DIR}/config/asterisk/extensions.conf.example"

usage() {
    cat <<'USAGE'
Usage:
  ./scripts/verify_asterisk.sh --samples
  sudo ./scripts/verify_asterisk.sh --runtime

--samples   Validate the repository's Asterisk example configuration.
--runtime   Validate samples, then query a running Asterisk instance for
            PJSIP, endpoints 1001/1002, AoRs, and the voip-lab dialplan.
USAGE
}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

require_file() {
    local path="$1"
    [[ -f "${path}" ]] || fail "missing file: ${path}"
}

require_fixed_text() {
    local path="$1"
    local text="$2"
    grep -Fq -- "${text}" "${path}" || fail "${path} is missing: ${text}"
}

verify_samples() {
    echo "== Asterisk sample configuration =="

    require_file "${PJSIP_SAMPLE}"
    require_file "${DIALPLAN_SAMPLE}"

    require_fixed_text "${PJSIP_SAMPLE}" "[transport-udp]"
    require_fixed_text "${PJSIP_SAMPLE}" "type=transport"
    require_fixed_text "${PJSIP_SAMPLE}" "protocol=udp"
    require_fixed_text "${PJSIP_SAMPLE}" "bind=0.0.0.0:5060"

    local endpoint
    for endpoint in 1001 1002; do
        local section_count
        section_count="$(grep -Fxc "[${endpoint}]" "${PJSIP_SAMPLE}")"
        [[ "${section_count}" -eq 2 ]] || \
            fail "expected endpoint and AoR sections named ${endpoint}; found ${section_count}"

        require_fixed_text "${PJSIP_SAMPLE}" "auth=auth${endpoint}"
        require_fixed_text "${PJSIP_SAMPLE}" "aors=${endpoint}"
        require_fixed_text "${PJSIP_SAMPLE}" "[auth${endpoint}]"
        require_fixed_text "${PJSIP_SAMPLE}" "username=${endpoint}"
        require_fixed_text "${DIALPLAN_SAMPLE}" "exten => ${endpoint},1,NoOp("
        require_fixed_text "${DIALPLAN_SAMPLE}" "PJSIP_DIAL_CONTACTS(${endpoint})"
    done

    require_fixed_text "${PJSIP_SAMPLE}" "context=voip-lab"
    require_fixed_text "${DIALPLAN_SAMPLE}" "[voip-lab]"

    echo "Asterisk sample configuration: PASS"
}

asterisk_cli() {
    local command="$1"
    asterisk -rx "${command}" 2>&1
}

verify_runtime() {
    echo "== Asterisk runtime =="

    command -v asterisk >/dev/null 2>&1 || \
        fail "asterisk is not installed; install and configure the lab first"

    local version_output
    if ! version_output="$(asterisk_cli "core show version")"; then
        printf '%s\n' "${version_output}" >&2
        fail "cannot connect to the Asterisk CLI; ensure Asterisk is running and retry with sudo"
    fi
    printf '%s\n' "${version_output}"

    local module_output
    module_output="$(asterisk_cli "module show like res_pjsip.so")"
    printf '%s\n' "${module_output}"
    grep -Fq "res_pjsip.so" <<<"${module_output}" || fail "res_pjsip.so is not loaded"

    local endpoint
    for endpoint in 1001 1002; do
        local endpoint_output
        endpoint_output="$(asterisk_cli "pjsip show endpoint ${endpoint}")"
        grep -Eq "Endpoint:.*${endpoint}" <<<"${endpoint_output}" || \
            fail "PJSIP endpoint ${endpoint} is not loaded"

        local aor_output
        aor_output="$(asterisk_cli "pjsip show aor ${endpoint}")"
        grep -Eq "Aor:.*${endpoint}" <<<"${aor_output}" || \
            fail "PJSIP AoR ${endpoint} is not loaded"
    done

    local dialplan_output
    dialplan_output="$(asterisk_cli "dialplan show voip-lab")"
    grep -Fq "1001" <<<"${dialplan_output}" || fail "voip-lab dialplan is missing extension 1001"
    grep -Fq "1002" <<<"${dialplan_output}" || fail "voip-lab dialplan is missing extension 1002"

    echo "Asterisk runtime configuration: PASS"
    echo
    echo "Registration/contact state:"
    asterisk_cli "pjsip show contacts"
}

mode="${1:---samples}"
case "${mode}" in
    --samples)
        verify_samples
        ;;
    --runtime)
        verify_samples
        verify_runtime
        ;;
    --help|-h)
        usage
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac
