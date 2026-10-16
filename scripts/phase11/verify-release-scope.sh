#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

require_file() {
    [[ -f "$1" ]] || fail "required file missing: $1"
}

require_dir() {
    [[ -d "$1" ]] || fail "required directory missing: $1"
}

echo '== Phase 11 release baseline =='

for command in git bash; do
    command -v "$command" >/dev/null 2>&1 || fail "required command not found: $command"
done

require_file CMakeLists.txt
require_file CMakePresets.json
require_file scripts/phase10/verify.sh
require_file scripts/quality/verify-docs-structure.sh
require_file scripts/quality/verify-evidence-structure.sh
require_file docs/phase10/README.md
require_file docs/architecture/rtsp.md
require_file evidence/phase10/README.md
require_dir tests/unit
require_dir tests/integration

[[ ! -e tests/phase10 ]] || fail 'legacy tests/phase10 directory must not exist'

if git grep -n 'tests/phase10' -- tests scripts/phase10 docs/phase10 docs/architecture/rtsp.md evidence/phase10 >/tmp/phase11-stale-test-layout.txt 2>/dev/null; then
    cat /tmp/phase11-stale-test-layout.txt >&2
    rm -f /tmp/phase11-stale-test-layout.txt
    fail 'stale tests/phase10 reference found'
fi
rm -f /tmp/phase11-stale-test-layout.txt

PHASE10_TAG='v0.10-rtsp-lab'
git rev-parse -q --verify "refs/tags/${PHASE10_TAG}^{commit}" >/dev/null || \
    fail "required published baseline tag missing: ${PHASE10_TAG}"

phase10_commit="$(git rev-parse "refs/tags/${PHASE10_TAG}^{commit}")"
git merge-base --is-ancestor "$phase10_commit" HEAD || \
    fail "HEAD does not descend from ${PHASE10_TAG}"

if ! grep -Fq 'Completed. Phase 10 established the RTSP/SDP/RTP mini-lab' evidence/phase10/README.md; then
    fail 'Phase 10 evidence is not in the expected completed state'
fi

echo '== documentation contracts =='
bash scripts/quality/verify-docs-structure.sh
bash scripts/quality/verify-evidence-structure.sh

echo '== Phase 11 shell syntax =='
while IFS= read -r -d '' script; do
    bash -n "$script"
done < <(find scripts/phase11 -maxdepth 1 -type f -name '*.sh' -print0 | sort -z)

echo "Phase 10 release baseline: ${PHASE10_TAG} @ ${phase10_commit}"
echo 'Phase 11 release scaffold verification: PASS'
