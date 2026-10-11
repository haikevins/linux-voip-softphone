#!/usr/bin/env bash
# Read-only source/evidence inventory for Phase 8 roadmap handoff.
# Does not change files, run live SIP calls, or authorize production shutdown.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

EXPECTED_HEAD="e98bc3418f76b15b4b568d603f5e4002cbd3a544"

echo '===== VERIFY PHASE 8 BASELINE ====='
test "$(git branch --show-current)" = 'phase/8-multithreading'
# The reviewed Commit 19 baseline must exist in the current history.
# An equality check would make the verifier unusable immediately after the
# roadmap closure commit is created. This check remains read-only.
git merge-base --is-ancestor "$EXPECTED_HEAD" HEAD
# The only permitted changes are the exact closure overlay. Caller must
# independently verify byte-for-byte overlay content before trusting results.

printf '\n===== VERIFY ROADMAP TEXT =====\n'
test -s README_roadmap.md
grep -Fq 'Phase 8 — Multithreading' README_roadmap.md
grep -Fq 'evidence/07-threading/' README_roadmap.md
grep -Fq 'Definition of Done' README_roadmap.md

printf '\n===== VERIFY REQUIRED ROADMAP ARTIFACTS =====\n'
for path in \
    docs/architecture/threading-model.md \
    evidence/07-threading/stress-test.txt \
    evidence/07-threading/tsan-report.txt \
    evidence/07-threading/threading-diagram.png \
    docs/architecture/threading-diagram.dot \
    docs/phase8/roadmap-exit-review.md; do
    test -s "$path"
    printf 'FOUND: %s\n' "$path"
done
file --mime-type evidence/07-threading/threading-diagram.png \
    | grep -Fq 'image/png'

printf '\n===== VERIFY AS-BUILT SOURCE CONTRACTS =====\n'
test -s src/concurrency/ThreadSafeQueue.hpp
test -s src/concurrency/SerialTaskWorker.hpp
test -s tests/phase8/test_mpmc_stress.cpp
grep -Fq "25'000" tests/phase8/test_mpmc_stress.cpp
grep -Fq 'constexpr std::size_t producers = 4;' tests/phase8/test_mpmc_stress.cpp
grep -Fq 'constexpr std::size_t consumers = 4;' tests/phase8/test_mpmc_stress.cpp
grep -Fq 'QMetaObject::invokeMethod' src/gui/QtRegistrationBackend.cpp
grep -Fq 'Qt::QueuedConnection' src/gui/QtRegistrationBackend.cpp
grep -Fq 'owner_thread_' src/application/CallManager.cpp
! grep -Eq 'SerialTaskWorker|ThreadSafeQueue' src/gui/QtRegistrationBackend.cpp

echo 'PASS: source boundaries and roadmap artifact inventory'

printf '\n===== VERIFY SHUTDOWN BLOCKER IS NOT LIFTED =====\n'
check_output="$(python3 -B scripts/phase8/audit-pjsip-shutdown-integration.py 2>&1)"
printf '%s\n' "$check_output"
grep -Fq 'Integration readiness: BLOCKED' <<<"$check_output"
grep -Fq 'Live external callback quiescence: UNPROVEN' <<<"$check_output"
grep -Fq 'Actual PJ_EBUSY recovery: UNPROVEN' <<<"$check_output"

# The static integration audit above remains production-integration scoped.
# Validate the separately recorded live vendor-behavior observation without
# promoting it into an external callback-quiescence proof.
printf '\n===== VERIFY CONTROLLED LIVE PJSUA2 EVIDENCE =====\n'
bash scripts/phase8/verify-pjsip.sh live-evidence

# A passing exit audit explicitly does NOT mean live PJSUA2 safety.
echo 'PASS: Phase 8 roadmap artifact and source-contract inventory'
echo 'STATUS: maintainer acceptance of Phase 8 still required'
