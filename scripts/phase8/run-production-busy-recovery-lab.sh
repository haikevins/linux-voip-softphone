#!/usr/bin/env bash
# One-shot, opt-in, REAL SIP P8-R2B lab with a production-owned Account.
# Never run this script from CI, without an operator, or in a production PBX.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$root"
fail() { printf 'ABORT: %s\n' "$*" >&2; exit 1; }

[[ "${PHASE8_LIVE_SIP:-}" == '1' ]] || fail 'explicit live SIP approval required: PHASE8_LIVE_SIP=1'
[[ "${PHASE8_R2B_CONFIRM:-}" == '1002-to-1001-active-shutdown' ]] || \
    fail 'explicit active-call shutdown approval required: PHASE8_R2B_CONFIRM=1002-to-1001-active-shutdown'

for tool in cmake ninja timeout pkg-config git; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing required tool: $tool"
done

export PKG_CONFIG_PATH="${PHASE8_PJSIP_PKGCONFIG:-$HOME/.local/pjproject-2.17/lib/pkgconfig}${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
pj_version="$(pkg-config --modversion libpjproject 2>/dev/null || true)"
[[ "$pj_version" == '2.17' ]] || fail 'requires verified PJPROJECT 2.17'

if [[ -z "${VOIP_SIP_PASSWORD:-}" ]]; then
    [[ -t 0 ]] || fail 'password missing and stdin is not interactive'
    read -rsp 'SIP password for lab extension 1002: ' VOIP_SIP_PASSWORD
    printf '\n'
    [[ -n "$VOIP_SIP_PASSWORD" ]] || fail 'empty SIP password'
    export VOIP_SIP_PASSWORD
fi

umask 077
ulimit -c 0 || true
raw="$(mktemp "${TMPDIR:-/tmp}/phase8-r2b-raw.XXXXXXXX")"
trap 'rm -f -- "$raw"; unset VOIP_SIP_PASSWORD' EXIT

build="$root/build/phase8-r2b-live"
printf '===== P8-R2B LIVE LAB BUILD =====\n'
cmake -S "$root" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DENABLE_PJSIP=ON -DENABLE_QT_GUI=OFF \
    -DENABLE_PHASE8_R2A_LAB=OFF -DENABLE_PHASE8_R2B_LAB=ON -DBUILD_TESTING=OFF
cmake --build "$build" --target phase8_r2b_production_busy_recovery -j 2
bin="$build/src/phase8_r2b_production_busy_recovery"
[[ -x "$bin" ]] || fail 'lab executable is missing'

log="${PHASE8_R2B_LOG:-$HOME/Downloads/phase8-r2b-$(date +%Y%m%d-%H%M%S).txt}"
mkdir -p -- "$(dirname -- "$log")"
[[ ! -e "$log" && ! -L "$log" ]] || fail "refusing to overwrite: $log"

printf '===== P8-R2B REAL PRODUCTION ACCOUNT OBSERVATION =====\n'
printf 'One attempt: 1002 -> 1001 on Asterisk 192.168.1.23 (UDP).\n'
printf 'This deliberately attempts non-forced account shutdown with an active call.\n'
printf 'The target might ring. No automatic retries or raw SIP evidence retained.\n'

set +e
timeout --signal=TERM --kill-after=5s 90s "$bin" >"$raw" 2>&1
rc=$?
set -e

# Restrict all retained text to fixed, credential-free lab markers. No raw
# traces, vendor error.info(), auth challenges or password are persisted.
set -C
{
    printf 'P8R2B_SOURCE_HEAD=%s\n' "$(git rev-parse --short HEAD)"
    if [[ -n "$(git status --porcelain --untracked-files=all)" ]]; then
        printf 'P8R2B_SOURCE_TREE=DIRTY_PRECOMMIT\n'
    else
        printf 'P8R2B_SOURCE_TREE=CLEAN\n'
    fi
    printf 'P8R2B_PJPROJECT=2.17\n'
    printf 'P8R2B_ENVIRONMENT=ASTERISK_192.168.1.23_1002_TO_1001_UDP\n'
    grep -E '^P8R2B_(FIRST_REGISTRATION|CALL_ATTEMPT|FIRST_SHUTDOWN|OWNERS_RETAINED|CALL_DISCONNECTED|STOP_RETRY|SAME_OBJECT_RESTART|SECOND_REGISTRATION|FINAL_STOP|DESTRUCTOR|RESULT)=(PASS|FAIL|STARTED|PJ_EBUSY|PRECONDITIONS_NOT_MET|UNEXPECTED_SUCCESS|OTHER_ERROR|RETURNED|INCONCLUSIVE|FAIL_EXCEPTION|FAIL_UNKNOWN)$' "$raw" || true
    printf 'P8R2B_PROCESS_EXIT=%s\n' "$rc"
    printf 'P8R2B_SCOPE=PRODUCTION_OUTGOING_LAB_EARLY_SHUTDOWN_ONLY\n'
    printf 'P8R2B_EXTERNAL_CALLBACK_QUIESCENCE=UNPROVEN\n'
    printf 'P8R2B_INTEGRATION_READINESS=BLOCKED\n'
} >"$log"
set +C

cat -- "$log"
printf 'P8R2B_SUMMARY_LOG=%s\n' "$log"

required=(
    'P8R2B_FIRST_REGISTRATION=PASS'
    'P8R2B_CALL_ATTEMPT=STARTED'
    'P8R2B_FIRST_SHUTDOWN=PJ_EBUSY'
    'P8R2B_OWNERS_RETAINED=PASS'
    'P8R2B_CALL_DISCONNECTED=PASS'
    'P8R2B_STOP_RETRY=RETURNED'
    'P8R2B_SAME_OBJECT_RESTART=PASS'
    'P8R2B_SECOND_REGISTRATION=PASS'
    'P8R2B_FINAL_STOP=RETURNED'
    'P8R2B_DESTRUCTOR=RETURNED'
    'P8R2B_RESULT=PASS'
    'P8R2B_PROCESS_EXIT=0'
)
if [[ "$rc" -ne 0 ]]; then
    fail 'live result FAIL/INCONCLUSIVE; preserve the sanitized summary and do not auto-repeat'
fi
for expected in "${required[@]}"; do
    [[ "$(grep -Fxc -- "$expected" "$log" || true)" == '1' ]] || \
        fail "missing or duplicate success marker: $expected"
done
printf 'PASS: bounded production-owned PJ_EBUSY/recovery observation; external quiescence still UNPROVEN\n'
