#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EVIDENCE_DIR="${ROOT_DIR}/evidence/phase4/registration"

usage() {
    cat <<'USAGE'
Usage:
  sudo ./scripts/phase4/verify-registration.sh --check
  sudo ./scripts/phase4/verify-registration.sh --capture

--check     Verify that Asterisk has registered contacts for SIP endpoints
            1001 and 1002, then print the current contact table.
--capture   Run the same verification and record non-secret Asterisk state
            under evidence/phase4/registration/.
USAGE
}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

asterisk_cli() {
    local command="$1"
    asterisk -rx "${command}" 2>&1
}

require_asterisk_runtime() {
    command -v asterisk >/dev/null 2>&1 || \
        fail "asterisk is not installed"

    local version_output
    if ! version_output="$(asterisk_cli "core show version")"; then
        printf '%s\n' "${version_output}" >&2
        fail "cannot connect to the Asterisk CLI; ensure Asterisk is running and retry with sudo"
    fi

    local module_output
    module_output="$(asterisk_cli "module show like res_pjsip.so")"
    grep -Fq "res_pjsip.so" <<<"${module_output}" || \
        fail "res_pjsip.so is not loaded"
}

verify_endpoint_and_contact() {
    local endpoint="$1"

    local endpoint_output
    endpoint_output="$(asterisk_cli "pjsip show endpoint ${endpoint}")"
    grep -Eq "Endpoint:[[:space:]]+${endpoint}([[:space:]]|$)" <<<"${endpoint_output}" || \
        fail "PJSIP endpoint ${endpoint} is not loaded"

    local aor_output
    aor_output="$(asterisk_cli "pjsip show aor ${endpoint}")"
    grep -Eq "Aor:[[:space:]]+${endpoint}([[:space:]]|$)" <<<"${aor_output}" || \
        fail "PJSIP AoR ${endpoint} is not loaded"

    grep -Eq "Contact:[[:space:]]+${endpoint}/" <<<"${aor_output}" || \
        fail "endpoint ${endpoint} has no registered contact"

    printf 'Endpoint %s registration: PASS\n' "${endpoint}"
}

verify_registration() {
    echo "== SIP registration verification =="
    require_asterisk_runtime

    verify_endpoint_and_contact 1001
    verify_endpoint_and_contact 1002

    local contacts_output
    contacts_output="$(asterisk_cli "pjsip show contacts")"
    grep -Eq "Contact:[[:space:]]+1001/" <<<"${contacts_output}" || \
        fail "global contact table is missing endpoint 1001"
    grep -Eq "Contact:[[:space:]]+1002/" <<<"${contacts_output}" || \
        fail "global contact table is missing endpoint 1002"

    echo
    printf '%s\n' "${contacts_output}"
    echo
    echo "SIP registration verification: PASS"
}

capture_evidence() {
    mkdir -p "${EVIDENCE_DIR}"

    local captured_at
    local branch
    local commit
    captured_at="$(date --iso-8601=seconds)"
    branch="$(git -C "${ROOT_DIR}" branch --show-current 2>/dev/null || true)"
    commit="$(git -C "${ROOT_DIR}" rev-parse HEAD 2>/dev/null || true)"

    asterisk_cli "core show version" >"${EVIDENCE_DIR}/asterisk-version.txt"
    asterisk_cli "pjsip show endpoints" >"${EVIDENCE_DIR}/endpoints.txt"
    asterisk_cli "pjsip show aors" >"${EVIDENCE_DIR}/aors.txt"
    asterisk_cli "pjsip show contacts" >"${EVIDENCE_DIR}/contacts.txt"

    {
        echo "Phase 4 SIP registration evidence"
        echo "Captured at: ${captured_at}"
        echo "Branch: ${branch:-unknown}"
        echo "Commit: ${commit:-unknown}"
        echo "Verified registered endpoints: 1001, 1002"
        echo "Verification command: sudo ./scripts/phase4/verify-registration.sh --check"
    } >"${EVIDENCE_DIR}/registration-summary.txt"

    local evidence_file
    local tmp_file
    while IFS= read -r -d "" evidence_file; do
        tmp_file="${evidence_file}.tmp"
        sed "s/[[:space:]]\+$//" "${evidence_file}" |
            tac |
            sed "/./,\$!d" |
            tac >"${tmp_file}"
        mv "${tmp_file}" "${evidence_file}"
    done < <(find "${EVIDENCE_DIR}" -maxdepth 1 -type f -name "*.txt" -print0)

    if [[ -n "${SUDO_UID:-}" && -n "${SUDO_GID:-}" ]]; then
        chown -R "${SUDO_UID}:${SUDO_GID}" "${EVIDENCE_DIR}"
    fi

    echo "Registration evidence written to: ${EVIDENCE_DIR}"
}

mode="${1:---check}"
case "${mode}" in
    --check)
        verify_registration
        ;;
    --capture)
        verify_registration
        capture_evidence
        ;;
    --help|-h)
        usage
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac
