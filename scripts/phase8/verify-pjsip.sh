#!/usr/bin/env bash
# Consolidated Phase 8 Qt/PJSIP offline verification gates.
# No subcommand establishes universal external callback quiescence.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
FOCUSED="${PHASE8_BUILD_DIR:-$ROOT/build/phase8-queue-debug}"
CANONICAL="${PHASE8_CANONICAL_BUILD_DIR:-$ROOT/build/qt-pjsip-gui-debug}"
MODE="${1:-all-offline}"
PHASE8_REQUIRED_ANCESTOR="34931aa8445f89adb6b4a370e1a519363873419f"
export PYTHONDONTWRITEBYTECODE=1

usage() {
    cat >&2 <<EOF_USAGE
Usage: bash $0 <mode>
Modes:
  callback-activity
  callback-ingress-races
  cleanup-timeout
  production-lifetime-audit
  shutdown-authorization
  shutdown-failure-paths
  shutdown-integration-contract
  shutdown-order
  shutdown-ownership-policy
  qt-ownership
  live-evidence
  all-offline
EOF_USAGE
}

require_tools() {
    local tool
    for tool in "$@"; do
        command -v "$tool" >/dev/null 2>&1 || {
            printf 'ABORT: required tool missing: %s\n' "$tool" >&2
            exit 1
        }
    done
}

require_distinct_builds() {
    [[ "$FOCUSED" != "$CANONICAL" ]] || {
        echo 'ABORT: focused and canonical build directories must differ' >&2
        exit 1
    }
}

callback_activity() {
    require_tools cmake ctest
    test -f "$ROOT/src/pjsip/CallbackActivityGate.hpp"
    test -f "$ROOT/tests/unit/test_pjsip_callback_activity.cpp"

    printf '===== PHASE 8 CALLBACK ACTIVITY (STANDALONE) =====\n'
    cmake -S "$ROOT/cmake/phase8-focused" -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$FOCUSED" --target phase8_pjsip_callback_activity_tests --parallel 2
    ctest --test-dir "$FOCUSED" -R '^Phase8PjsipCallbackActivityTest$' --output-on-failure

    printf '===== PHASE 8 CALLBACK ACTIVITY (CANONICAL) =====\n'
    cmake -S "$ROOT" -B "$CANONICAL"
    cmake --build "$CANONICAL" --target phase8_pjsip_callback_activity_tests --parallel 2
    ctest --test-dir "$CANONICAL" -R '^Phase8PjsipCallbackActivityTest$' --output-on-failure

    printf 'PASS: Phase 8 callback activity model gate (standalone + canonical)\n'
    printf 'NOTE: no production SIP callback is instrumented by this test.\n'
}

callback_ingress_races() {
    require_tools cmake ctest
    require_distinct_builds
    test -f "$ROOT/src/pjsip/CallbackActivityGate.hpp"
    test -f "$ROOT/tests/unit/test_pjsip_callback_ingress_races.cpp"

    printf '===== SYNTHETIC EXTERNAL CALLBACK INGRESS (FOCUSED) =====\n'
    cmake -S "$ROOT/cmake/phase8-focused" -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$FOCUSED" --target phase8_pjsip_callback_ingress_races_tests -j 2
    ctest --test-dir "$FOCUSED" --output-on-failure -R '^Phase8PjsipCallbackIngressRacesTest$'

    printf '===== SYNTHETIC EXTERNAL CALLBACK INGRESS (CANONICAL) =====\n'
    [[ -f "$CANONICAL/CMakeCache.txt" ]] || {
        printf 'ABORT: canonical build directory is not configured: %s\n' "$CANONICAL" >&2
        exit 1
    }
    cmake -S "$ROOT" -B "$CANONICAL"
    cmake --build "$CANONICAL" --target phase8_pjsip_callback_ingress_races_tests -j 2
    ctest --test-dir "$CANONICAL" --output-on-failure -R '^Phase8PjsipCallbackIngressRacesTest$'

    printf '%s\n' 'PASS: Phase 8 synthetic PJSIP callback ingress race MODEL gate (live PJSUA2 NOT TESTED)'
}

cleanup_timeout() {
    require_tools cmake ctest
    cd "$ROOT"
    git merge-base --is-ancestor "$PHASE8_REQUIRED_ANCESTOR" HEAD || {
        echo "ABORT: Phase 8 baseline missing from current HEAD ancestry: $PHASE8_REQUIRED_ANCESTOR" >&2
        exit 1
    }
    [[ -f "$CANONICAL/CMakeCache.txt" ]] || {
        echo "ABORT: missing canonical CMakeCache.txt: $CANONICAL" >&2
        exit 1
    }
    grep -Fxq 'ENABLE_PJSIP:BOOL=ON' "$CANONICAL/CMakeCache.txt" || {
        echo 'ABORT: canonical build must enable PJSIP' >&2
        exit 1
    }

    cmake -S . -B "$CANONICAL"
    cmake --build "$CANONICAL" --target \
        voip_pjsip pjsip_config_tests phase8_pjsip_call_slot_cleanup_tests \
        -j"$(nproc)"
    ctest --test-dir "$CANONICAL" --output-on-failure -R '^Phase8PjsipCallSlotCleanupTest$'
    ctest --test-dir "$CANONICAL" --output-on-failure -L '^pjsip$'

    echo 'PASS: Phase 8 PJSIP polling policy and canonical PJSIP gate'
    echo 'NOTE: no live SIP callback or timeout-destruction safety verified'
}

production_lifetime_audit() {
    require_tools python3 cmake ctest
    cd "$ROOT"
    echo '===== SOURCE AUDIT (REAL PRODUCTION C++ FILES) ====='
    python3 -B scripts/phase8/audit-pjsip-production-lifetime.py

    echo '===== MUTATION-ORIENTED REGRESSIONS ====='
    python3 -B tests/integration/test_pjsip_production_lifetime_audit.py

    echo '===== FOCUSED CTEST REGISTRATION ====='
    cmake -S cmake/phase8-focused -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
    ctest --test-dir "$FOCUSED" --output-on-failure -R '^Phase8PjsipProductionAuditTest$'

    echo '===== CANONICAL CTEST REGISTRATION ====='
    cmake -S . -B "$CANONICAL"
    ctest --test-dir "$CANONICAL" --output-on-failure -R '^Phase8PjsipProductionAuditTest$'

    printf '%s\n' 'PASS: Phase 8 production callback source audit (static only; live quiescence INCONCLUSIVE)'
}

shutdown_authorization() {
    require_tools cmake ctest
    require_distinct_builds
    cd "$ROOT"
    test -f src/pjsip/PjsipShutdownAuthorization.hpp
    test -f tests/unit/test_pjsip_shutdown_authorization.cpp

    echo '===== PRODUCTION-HEADER AUTHORIZATION (FOCUSED) ====='
    cmake -S cmake/phase8-focused -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$FOCUSED" --target phase8_pjsip_shutdown_authorization_tests -j 2
    ctest --test-dir "$FOCUSED" --output-on-failure -R '^Phase8PjsipShutdownAuthorizationTest$'

    echo '===== PRODUCTION-HEADER AUTHORIZATION (CANONICAL) ====='
    [[ -f "$CANONICAL/CMakeCache.txt" ]] || {
        printf 'ABORT: canonical build directory not configured: %s\n' "$CANONICAL" >&2
        exit 1
    }
    cmake -S . -B "$CANONICAL"
    cmake --build "$CANONICAL" --target phase8_pjsip_shutdown_authorization_tests -j 2
    ctest --test-dir "$CANONICAL" --output-on-failure -R '^Phase8PjsipShutdownAuthorizationTest$'

    printf '%s\n' 'PASS: Phase 8 PJSIP shutdown authorization HEADER gate (live PJSUA2 NOT TESTED)'
}

shutdown_failure_paths() {
    require_tools python3 cmake ctest
    require_distinct_builds
    cd "$ROOT"

    echo '===== BOUNDED PRODUCTION SOURCE RISK INVENTORY ====='
    python3 -B - <<'PY'
import importlib.util
from pathlib import Path
import sys

root = Path.cwd()
audit_path = root / 'scripts/phase8/audit-pjsip-production-lifetime.py'
spec = importlib.util.spec_from_file_location('phase8_shutdown_inventory', audit_path)
if spec is None or spec.loader is None:
    raise SystemExit('ABORT: cannot load existing production source audit')
audit = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = audit
spec.loader.exec_module(audit)

outgoing = (root / 'src/pjsip/SipOutgoingCallClient.cpp').read_text(encoding='utf-8')
incoming = (root / 'src/pjsip/SipIncomingCallClient.cpp').read_text(encoding='utf-8')
result = audit.audit_pair(outgoing, incoming)
if result['source_contract'] != 'PASS':
    raise SystemExit('ABORT: production source structure changed; re-audit before proceeding')

for client, state in result['clients'].items():
    if state['risks']:
        raise SystemExit(
            f'ABORT: {client} source closure risk inventory is not empty: '
            f'{sorted(state["risks"])}'
        )
    print(f'{client}: 0 known source closure risks OPEN')

print('PASS: production source closure inventory (offline structural contract)')
print('Live PJSUA2 callback-target quiescence: NOT TESTED')
print('Actual PJ_EBUSY recovery: NOT TESTED')
PY

    echo '===== FOCUSED MODEL TARGET ====='
    cmake -S cmake/phase8-focused -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$FOCUSED" --target phase8_pjsip_shutdown_failure_paths_tests -j 2
    ctest --test-dir "$FOCUSED" --output-on-failure -R '^Phase8PjsipShutdownFailurePathsTest$'

    echo '===== CANONICAL MODEL TARGET ====='
    [[ -f "$CANONICAL/CMakeCache.txt" ]] || {
        printf 'ABORT: canonical build not configured: %s\n' "$CANONICAL" >&2
        exit 1
    }
    cmake -S . -B "$CANONICAL"
    cmake --build "$CANONICAL" --target phase8_pjsip_shutdown_failure_paths_tests -j 2
    ctest --test-dir "$CANONICAL" --output-on-failure -R '^Phase8PjsipShutdownFailurePathsTest$'

    printf '%s\n' 'PASS: Phase 8 PJSIP shutdown failure-path MODEL gate (live PJSUA2 NOT TESTED)'
}

shutdown_integration_contract() {
    require_tools python3 cmake ctest
    require_distinct_builds
    cd "$ROOT"

    echo '===== REAL PRODUCTION SHUTDOWN INTEGRATION BLOCKER INVENTORY ====='
    python3 -B scripts/phase8/audit-pjsip-shutdown-integration.py --source-root "$ROOT"

    echo '===== VERIFY READINESS FAILS CLOSED ====='
    if python3 -B scripts/phase8/audit-pjsip-shutdown-integration.py \
        --source-root "$ROOT" --require-ready >/dev/null; then
        printf '%s\n' 'ABORT: audit incorrectly declared real teardown READY' >&2
        exit 1
    else
        status=$?
        [[ "$status" -eq 3 ]] || {
            printf 'ABORT: unexpected readiness status: %s\n' "$status" >&2
            exit 1
        }
    fi
    printf '%s\n' 'PASS: actual PJSUA2 integration remains explicitly BLOCKED'

    echo '===== FOCUSED STATIC AUDIT CTEST ====='
    cmake -S cmake/phase8-focused -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
    ctest --test-dir "$FOCUSED" --output-on-failure -R '^Phase8PjsipShutdownIntegrationContractTest$'

    echo '===== CANONICAL STATIC AUDIT CTEST ====='
    [[ -f "$CANONICAL/CMakeCache.txt" ]] || {
        printf 'ABORT: canonical build not configured: %s\n' "$CANONICAL" >&2
        exit 1
    }
    cmake -S . -B "$CANONICAL"
    ctest --test-dir "$CANONICAL" --output-on-failure -R '^Phase8PjsipShutdownIntegrationContractTest$'

    printf '%s\n' 'PASS: Phase 8 PJSIP shutdown integration BLOCKER audit gate (live PJSUA2 NOT TESTED)'
}

shutdown_order() {
    require_tools python3 cmake ctest
    cd "$ROOT"
    git merge-base --is-ancestor "$PHASE8_REQUIRED_ANCESTOR" HEAD || {
        echo "ABORT: Phase 8 baseline missing from current HEAD ancestry: $PHASE8_REQUIRED_ANCESTOR" >&2
        exit 1
    }

    python3 -B -m unittest discover -s tests/integration -p test_pjsip_shutdown_order.py -v
    printf 'PASS: Phase 8 PJSIP source shutdown-order contract\n'

    if [[ "${PHASE8_PJSIP_STATIC_ONLY:-0}" == '1' ]]; then
        echo 'STATIC ONLY: PJSIP compilation and dynamic CTests NOT RUN.'
        return 0
    fi
    [[ "${PHASE8_PJSIP_STATIC_ONLY:-0}" == '0' ]] || {
        echo 'ABORT: PHASE8_PJSIP_STATIC_ONLY must be 0 or 1' >&2
        exit 1
    }

    [[ -f "$CANONICAL/CMakeCache.txt" ]] || {
        echo "ABORT: canonical build missing: $CANONICAL" >&2
        exit 1
    }
    grep -Fxq 'ENABLE_PJSIP:BOOL=ON' "$CANONICAL/CMakeCache.txt" || {
        echo 'ABORT: PJSIP support must be ON in canonical CMake cache' >&2
        exit 1
    }

    cmake --build "$CANONICAL" --target voip_pjsip pjsip_config_tests -j"$(nproc)"
    ctest --test-dir "$CANONICAL" --output-on-failure -L '^pjsip$'
    echo 'PASS: Phase 8 PJSIP shutdown-order build and CTest gate'
}

shutdown_ownership_policy() {
    require_tools cmake ctest
    require_distinct_builds
    cd "$ROOT"
    test -f tests/unit/test_pjsip_shutdown_ownership_policy.cpp
    test -f src/pjsip/CallbackActivityGate.hpp

    printf '===== FOCUSED SYNTHETIC POLICY TARGET =====\n'
    cmake -S cmake/phase8-focused -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$FOCUSED" --target phase8_pjsip_shutdown_ownership_policy_tests -j 2
    ctest --test-dir "$FOCUSED" --output-on-failure -R '^Phase8PjsipShutdownOwnershipPolicyTest$'

    printf '===== CANONICAL SYNTHETIC POLICY TARGET =====\n'
    [[ -f "$CANONICAL/CMakeCache.txt" ]] || {
        printf 'ABORT: canonical build directory is not configured: %s\n' "$CANONICAL" >&2
        exit 1
    }
    cmake -S . -B "$CANONICAL"
    cmake --build "$CANONICAL" --target phase8_pjsip_shutdown_ownership_policy_tests -j 2
    ctest --test-dir "$CANONICAL" --output-on-failure -R '^Phase8PjsipShutdownOwnershipPolicyTest$'

    printf '%s\n' 'PASS: Phase 8 PJSIP shutdown ownership POLICY MODEL gate (live PJSUA2 NOT TESTED)'
}

qt_ownership() {
    require_tools cmake ctest
    cd "$ROOT"
    git merge-base --is-ancestor "$PHASE8_REQUIRED_ANCESTOR" HEAD || {
        echo "ABORT: Phase 8 baseline missing from current HEAD ancestry: $PHASE8_REQUIRED_ANCESTOR" >&2
        exit 1
    }

    local build_dir="${PHASE8_QT_BUILD_DIR:-$CANONICAL}"
    [[ -f "$build_dir/CMakeCache.txt" ]] || {
        echo "ABORT: canonical Qt build directory not configured: $build_dir" >&2
        exit 1
    }
    grep -Eq '^ENABLE_QT_GUI:BOOL=ON$' "$build_dir/CMakeCache.txt" || {
        echo 'ABORT: selected build must enable Qt GUI' >&2
        exit 1
    }

    cmake --build "$build_dir" --target qt_registration_backend_tests -j"$(nproc)"
    ctest --test-dir "$build_dir" --output-on-failure -R '^QtRegistrationBackendTest$'
    echo 'PASS: Qt owner-thread and shutdown regression gate'
}

live_evidence() {
    cd "$ROOT"
    local evidence='evidence/phase8/pjsip-live-lifecycle.txt'
    local doc='docs/phase8/pjsip-live-lifecycle.md'
    local runner='scripts/phase8/run-pjsip-live-lifecycle-probe.sh'
    local source='labs/pjsip/live-lifecycle/pjsip_live_lifecycle_probe.cpp'
    local marker

    for path in "$evidence" "$doc" "$runner" "$source"; do
        [[ -s "$path" ]] || {
            printf 'ERROR: missing or empty required artifact: %s\n' "$path" >&2
            exit 1
        }
    done

    local required_markers=(
        'BASELINE_HEAD=4ace2be'
        'PJSIP_VERSION=2.17'
        'REGISTERED=yes status=200'
        'ACTIVE_CALL_SLOTS=1'
        'FIRST_SHUTDOWN=PJ_EBUSY'
        'CALL_SLOTS_AFTER_HANGUP=0'
        'SECOND_SHUTDOWN=SUCCESS'
        'ACCOUNT_CALLBACKS_ACTIVE_AT_RETURN=0'
        'ACCOUNT_CALLBACKS_DURING_GRACE=0'
        'POST_SHUTDOWN_CALLBACKS=0'
        'LIB_DESTROY=SUCCESS'
        'POST_LIBDESTROY_CALLBACKS=0'
        'PROBE_RESULT=PASS'
        'ORIGINAL_TEMP_LOG_SHA256=77c62f614fef037c81aa254ebab2352a557197d2c278a6927a90084d93d481f5'
        'CONTROLLED_PJSUA2_PJ_EBUSY_RECOVERY=LIVE_OBSERVED_EVIDENCED'
        'LIVE_EXTERNAL_CALLBACK_QUIESCENCE=UNPROVEN'
        'LATE_REGISTRATION_CALLBACKS_IN_RECORDED_RUN=NONE_OBSERVED'
        'PRODUCTION_INTEGRATION_READINESS=BLOCKED'
    )

    for marker in "${required_markers[@]}"; do
        grep -Fxq "$marker" "$evidence" || {
            printf 'ERROR: missing live evidence marker: %s\n' "$marker" >&2
            exit 1
        }
    done

    if grep -Ein \
        '^[[:space:]]*(Proxy-)?Authorization:[[:space:]]*Digest|nonce="|cnonce="|response="|a=crypto:.*inline:' \
        "$evidence" >/dev/null 2>&1; then
        echo 'ERROR: secret-bearing SIP/SRTP material detected in live evidence' >&2
        exit 1
    fi

    grep -Fq 'Live external callback quiescence: **UNPROVEN**' "$doc"
    grep -Fq 'Production integration readiness: **BLOCKED**' "$doc"
    grep -Fq 'controlled PJSUA2 `PJ_EBUSY` recovery sequence' "$doc"

    grep -Fq 'PHASE8_LIVE_SIP' "$runner"
    grep -Fq 'pkg-config --libs --static libpjproject' "$runner"
    grep -Fq 'shutdown.force = false;' "$source"
    grep -Fq 'FIRST_SHUTDOWN=PJ_EBUSY' "$source"
    grep -Fq 'pjsua_call_get_count() == 0U' "$source"

    printf '%s\n' 'PASS: recorded controlled PJSUA2 live lifecycle evidence'
    printf '%s\n' 'Controlled PJSUA2 PJ_EBUSY recovery: LIVE OBSERVED / EVIDENCED'
    printf '%s\n' 'Late registration callbacks after shutdown2: NONE OBSERVED IN RECORDED LIVE RUN'
    printf '%s\n' 'Live external callback quiescence: UNPROVEN'
    printf '%s\n' 'Production integration readiness: BLOCKED'
}

case "$MODE" in
    callback-activity) callback_activity ;;
    callback-ingress-races) callback_ingress_races ;;
    cleanup-timeout) cleanup_timeout ;;
    production-lifetime-audit) production_lifetime_audit ;;
    shutdown-authorization) shutdown_authorization ;;
    shutdown-failure-paths) shutdown_failure_paths ;;
    shutdown-integration-contract) shutdown_integration_contract ;;
    shutdown-order) shutdown_order ;;
    shutdown-ownership-policy) shutdown_ownership_policy ;;
    qt-ownership) qt_ownership ;;
    live-evidence) live_evidence ;;
    all-offline)
        callback_activity
        callback_ingress_races
        cleanup_timeout
        production_lifetime_audit
        shutdown_authorization
        shutdown_failure_paths
        shutdown_integration_contract
        shutdown_order
        shutdown_ownership_policy
        qt_ownership
        live_evidence
        ;;
    *)
        usage
        exit 2
        ;;
esac
