#!/usr/bin/env bash
# Explicit, single-run P8-R2A production-client live stop/reuse observation.
# No SIP traffic unless two opt-in variables are present.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$root"
fail() { printf 'ABORT: %s\n' "$*" >&2; exit 1; }

[[ "${PHASE8_LIVE_SIP:-}" == '1' ]] || fail 'set PHASE8_LIVE_SIP=1 to approve real SIP traffic'
[[ "${PHASE8_R2A_CONFIRM:-}" == '1002-to-1001' ]] || \
    fail 'set PHASE8_R2A_CONFIRM=1002-to-1001 to approve this lab destination'
command -v cmake >/dev/null 2>&1 || fail 'cmake not found'
command -v timeout >/dev/null 2>&1 || fail 'GNU timeout not found'
command -v pkg-config >/dev/null 2>&1 || fail 'pkg-config not found'
command -v git >/dev/null 2>&1 || fail 'git not found'

export PKG_CONFIG_PATH="${PHASE8_PJSIP_PKGCONFIG:-$HOME/.local/pjproject-2.17/lib/pkgconfig}${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
pj_version="$(pkg-config --modversion libpjproject 2>/dev/null || true)"
[[ "$pj_version" == '2.17' ]] || \
    fail 'requires the recorded PJPROJECT 2.17 environment'

if [[ -z "${VOIP_SIP_PASSWORD:-}" ]]; then
    [[ -t 0 ]] || fail 'VOIP_SIP_PASSWORD is unset and stdin is not interactive'
    read -rsp 'SIP password for lab extension 1002: ' VOIP_SIP_PASSWORD
    printf '\n'
    [[ -n "$VOIP_SIP_PASSWORD" ]] || fail 'empty SIP password'
    export VOIP_SIP_PASSWORD
fi

umask 077
ulimit -c 0 || true
raw="$(mktemp "${TMPDIR:-/tmp}/phase8-r2a-raw.XXXXXXXX")"
trap 'rm -f -- "$raw"; unset VOIP_SIP_PASSWORD' EXIT

build="$root/build/phase8-r2a-live"
printf '===== P8-R2A LIVE LAB BUILD =====\n'
cmake -S "$root" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DENABLE_PJSIP=ON -DENABLE_QT_GUI=OFF \
    -DENABLE_PHASE8_R2A_LAB=ON -DBUILD_TESTING=OFF
cmake --build "$build" --target phase8_r2a_production_stop_reuse -j 2
bin="$build/src/phase8_r2a_production_stop_reuse"
[[ -x "$bin" ]] || fail 'live lab executable missing'

log="${PHASE8_R2A_LOG:-$HOME/Downloads/phase8-r2a-$(date +%Y%m%d-%H%M%S).txt}"
mkdir -p -- "$(dirname -- "$log")"
[[ ! -e "$log" && ! -L "$log" ]] || fail "summary log already exists: $log"

printf '===== P8-R2A LIVE PRODUCTION-CLIENT OBSERVATION =====\n'
printf 'Scope: Asterisk 192.168.1.23, account 1002, target 1001 (UDP).\n'
printf 'The target may ring. No automatic repeats will be performed.\n'

set +e
timeout --signal=TERM --kill-after=5s 75s "$bin" >"$raw" 2>&1
rc=$?
set -e

# Copy only predefined, bounded, credential-free markers. Never retain raw
# PJSIP traces, stdout, Digest fields, error.info(), or the SIP password.
{
    printf 'P8R2A_SOURCE_HEAD=%s\n' "$(git rev-parse --short HEAD)"
    if [[ -n "$(git status --porcelain --untracked-files=all)" ]]; then
        printf 'P8R2A_SOURCE_TREE=DIRTY_PRECOMMIT\n'
    else
        printf 'P8R2A_SOURCE_TREE=CLEAN\n'
    fi
    printf 'P8R2A_PJPROJECT=2.17\n'
    printf 'P8R2A_ENVIRONMENT=ASTERISK_192.168.1.23_1002_TO_1001_UDP\n'
    grep -E '^P8R2A_(FIRST_REGISTRATION|CALL_ATTEMPT|CALL_RESPONSE|CALL_DISCONNECTED|FIRST_STOP|SAME_OBJECT_RESTART|SECOND_REGISTRATION|SECOND_STOP|DESTRUCTOR|PJ_EBUSY|RESULT)=(PASS|FAIL|STARTED|PENDING|COMPLETED|REJECTED|RETURNED|NOT_EXERCISED|FAIL_EXCEPTION|FAIL_UNKNOWN)$' "$raw" || true
    printf 'P8R2A_PROCESS_EXIT=%s\n' "$rc"
    printf 'P8R2A_SCOPE=PRODUCTION_OUTGOING_STOP_REUSE_ONLY\n'
    printf 'P8R2A_REAL_PJ_EBUSY_RECOVERY=UNPROVEN\n'
    printf 'P8R2A_EXTERNAL_CALLBACK_QUIESCENCE=UNPROVEN\n'
} >"$log"

cat "$log"
printf 'P8R2A_SUMMARY_LOG=%s\n' "$log"

if [[ "$rc" -ne 0 ]] || ! grep -Fxq 'P8R2A_RESULT=PASS' "$log" || \
   ! grep -Fxq 'P8R2A_DESTRUCTOR=RETURNED' "$log" || \
   ! grep -Fxq 'P8R2A_SAME_OBJECT_RESTART=PASS' "$log"; then
    fail 'live observation failed/inconclusive; raw SIP trace deliberately discarded'
fi
printf 'PASS: P8-R2A production-client stop/reuse observation (NOT PJ_EBUSY recovery)\n'
