#!/usr/bin/env bash
# Phase 8 final acceptance: actual ThinkPad runtime; NOT live SIP/PJSUA2 safety.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT"
EXPECTED_ANCESTOR="0cd1a7bdb6b0de8e9ed6d1efef0d9722309436a7"
MODE="${1:---automatic}"
case "$MODE" in
    --automatic|--manual) ;;
    *) printf 'Usage: bash %s [--automatic|--manual]\n' "$0" >&2; exit 2 ;;
esac

command -v git >/dev/null
command -v python3 >/dev/null
[[ "$(git branch --show-current)" == 'phase/8-multithreading' ]] || {
    echo 'ABORT: wrong git branch' >&2; exit 1;
}
git merge-base --is-ancestor "$EXPECTED_ANCESTOR" HEAD || {
    echo 'ABORT: phase8 closure baseline missing from current HEAD ancestry' >&2; exit 1;
}

BUILD="$ROOT/build/qt-pjsip-gui-debug"
GUI="$BUILD/src/voip-qt-gui"
[[ -f scripts/phase8/verify-roadmap-exit.sh ]]
[[ -f scripts/phase8/verify-threading.sh ]]
[[ -f scripts/phase8/verify-pjsip.sh ]]
[[ -f scripts/phase8/gui-runtime-probe.py ]]
[[ -f tests/phase8/test_gui_runtime_acceptance_harness.py ]]

LOG_DIR="${PHASE8_ACCEPT_LOG_DIR:-$(mktemp -d /tmp/phase8-runtime-acceptance.XXXXXXXX)}"
mkdir -p -- "$LOG_DIR"
LOG_DIR="$(realpath -- "$LOG_DIR")"
case "$LOG_DIR" in
    "$ROOT"|"$ROOT"/*)
        echo 'ABORT: acceptance logs must be outside the source checkout' >&2
        exit 1
        ;;
esac
printf 'EVIDENCE_DIR=%s\n' "$LOG_DIR"

# A dedicated live log is never checked into the repository automatically.
if [[ "$MODE" == '--manual' ]]; then
    [[ -x "$GUI" ]] || { echo "ABORT: GUI executable missing: $GUI" >&2; exit 1; }
    echo '===== MANUAL GUI USABILITY / IDLE SOAK ====='
    PYTHONUNBUFFERED=1 python3 -B scripts/phase8/gui-runtime-probe.py manual \
        --binary "$GUI" --log-dir "$LOG_DIR" \
        --observe-seconds "${PHASE8_MANUAL_OBSERVE_SECONDS:-90}" \
        --close-timeout "${PHASE8_MANUAL_CLOSE_TIMEOUT:-45}" \
        2>&1 | tee "$LOG_DIR/manual.log"
    echo 'PASS: Phase 8 manual GUI runtime gate (no SIP, human confirmed)'
    echo "EVIDENCE_DIR=$LOG_DIR"
    exit 0
fi

# Everything below is deterministic and offline: no SIP credentials or calls.
for cmd in cmake ctest clang++-18; do
    command -v "$cmd" >/dev/null || { echo "ABORT: missing $cmd" >&2; exit 1; }
done

printf '\n===== 1. CANONICAL QT/PJSIP CONFIGURE+BUILD =====\n'
cmake -S . -B "$BUILD" -DENABLE_QT_GUI=ON -DENABLE_PJSIP=ON \
    2>&1 | tee "$LOG_DIR/configure.log"
cmake --build "$BUILD" --parallel 4 \
    2>&1 | tee "$LOG_DIR/build.log"
[[ -x "$GUI" ]]
for feature in ENABLE_QT_GUI ENABLE_PJSIP; do
    grep -Fxq "$feature:BOOL=ON" "$BUILD/CMakeCache.txt" || {
        echo "ABORT: canonical build missing $feature=ON" >&2; exit 1;
    }
done
ctest --test-dir "$BUILD" -N | tee "$LOG_DIR/ctest-inventory.log"
grep -Fxq 'Total Tests: 147' "$LOG_DIR/ctest-inventory.log" || {
    echo 'ABORT: expected canonical 147 tests; review before proceeding' >&2
    exit 1
}

printf '\n===== 2. CANONICAL CTEST =====\n'
ctest --test-dir "$BUILD" --output-on-failure --timeout 60 \
    2>&1 | tee "$LOG_DIR/canonical.log"
grep -Fq '100% tests passed, 0 tests failed out of 147' "$LOG_DIR/canonical.log"
echo 'PASS: canonical 147/147'

printf '\n===== 3. QT WORKER THREAD HANDOFF 10 REPEATS =====\n'
ctest --test-dir "$BUILD" --output-on-failure --timeout 30 \
    -R '^QtRegistrationBackendTest$' --repeat until-fail:10 \
    2>&1 | tee "$LOG_DIR/qt-worker-repeat.log"
echo 'PASS: Qt worker regression x10 (fake session; no live PJSIP)'

printf '\n===== 4. PHASE 8 FOCUSED QUEUE/WORKER =====\n'
bash scripts/phase8/verify-threading.sh queue 2>&1 | tee "$LOG_DIR/phase8-focused.log"
grep -Fq '100% tests passed, 0 tests failed out of 12' "$LOG_DIR/phase8-focused.log"
echo 'PASS: Phase 8 focused 12/12'

printf '\n===== 5. 100,000-EVENT MPMC STRESS =====\n'
PHASE8_STRESS_REPEATS="${PHASE8_ACCEPT_STRESS_REPEATS:-30}" \
    bash scripts/phase8/verify-threading.sh stress \
    2>&1 | tee "$LOG_DIR/mpmc-stress.log"
echo 'PASS: MPMC stress repeated'

printf '\n===== 6. CLANG18 TSAN ISOLATED =====\n'
CXX="$(command -v clang++-18)" \
PHASE8_TSAN_BUILD_DIR="$ROOT/build/phase8-tsan-clang18-debug" \
PHASE8_TSAN_REPEATS=3 \
    bash scripts/phase8/verify-threading.sh tsan \
    2>&1 | tee "$LOG_DIR/tsan.log"
grep -Fq 'PASS: Phase 8 TSan gate (six tests x 3 run(s))' "$LOG_DIR/tsan.log"
echo 'PASS: Clang18 TSan 18/18'

printf '\n===== 7. PJSIP NON-LIVE REGRESSION =====\n'
bash scripts/phase8/verify-pjsip.sh shutdown-order \
    2>&1 | tee "$LOG_DIR/pjsip-nonlive.log"
grep -Fq '100% tests passed, 0 tests failed out of 45' "$LOG_DIR/pjsip-nonlive.log"
echo 'PASS: PJSIP non-live 45/45'

printf '\n===== 8. ROADMAP SOURCE/AUDIT GATE =====\n'
bash scripts/phase8/verify-roadmap-exit.sh 2>&1 | tee "$LOG_DIR/roadmap-audit.log"
grep -Fq 'Integration readiness: BLOCKED' "$LOG_DIR/roadmap-audit.log"
grep -Fq 'PASS: Phase 8 roadmap artifact and source-contract inventory' "$LOG_DIR/roadmap-audit.log"
echo 'PASS: roadmap artifact gate (production SIP shutdown still BLOCKED)'

printf '\n===== 9. SELF-TEST RUNTIME ACCEPTANCE HARNESS =====\n'
python3 -B -m unittest discover -s tests/phase8 \
    -p 'test_gui_runtime_acceptance_harness.py' -v \
    2>&1 | tee "$LOG_DIR/harness-unit.log"
echo 'PASS: runtime probe unit/mutation tests'

printf '\n===== 10. REPEATED QT/QML STARTUP & NORMAL EXIT =====\n'
python3 -B scripts/phase8/gui-runtime-probe.py smoke \
    --binary "$GUI" \
    --log-dir "$LOG_DIR" \
    --iterations "${PHASE8_ACCEPT_GUI_REPEATS:-40}" \
    --timeout "${PHASE8_ACCEPT_GUI_TIMEOUT:-8}" \
    --slow-ms "${PHASE8_ACCEPT_GUI_SLOW_MS:-4000}" \
    2>&1 | tee "$LOG_DIR/gui-smoke.log"
echo 'PASS: repeated GUI smoke (headless, no SIP)'

printf '\n===== AUTOMATED ACCEPTANCE SUMMARY =====\n'
printf 'HEAD=%s\n' "$(git rev-parse HEAD)" | tee "$LOG_DIR/git-head.txt"
cat <<EOF | tee "$LOG_DIR/acceptance-status.txt"
AUTOMATED: PASS
MANUAL_GUI: PENDING (run --manual with graphical desktop)
LIVE_SIP: NOT TESTED
PJSUA2_SHUTDOWN_SAFETY: BLOCKED/UNPROVEN
EVIDENCE_DIR: $LOG_DIR
EOF
printf 'PASS: PHASE 8 AUTOMATED FINAL RUNTIME ACCEPTANCE\n'
printf 'NEXT: PHASE8_ACCEPT_LOG_DIR=%q bash scripts/phase8/verify-final-runtime-acceptance.sh --manual\n' "$LOG_DIR"
