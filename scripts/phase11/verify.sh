#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

required_paths=(
    CMakeLists.txt
    CMakePresets.json
    scripts/phase10/verify.sh
    scripts/phase11/verify-release-scope.sh
    scripts/phase11/verify-package.sh
    scripts/phase11/verify-final-acceptance.sh
    scripts/quality/verify-docs-structure.sh
    scripts/quality/verify-evidence-structure.sh
    docs/phase11/README.md
    docs/architecture/system-overview.md
    docs/interview/phase11-questions.md
    docs/phase11/demo-guide.md
    evidence/phase11/README.md
    evidence/phase11/automated-verification.txt
    evidence/phase11/release-verification.txt
)

for path in "${required_paths[@]}"; do
    [[ -f "$path" ]] || fail "required Phase 11 closure path missing: $path"
done

for path in \
    scripts/phase10/verify.sh \
    scripts/phase11/verify-release-scope.sh \
    scripts/phase11/verify-package.sh \
    scripts/phase11/verify-final-acceptance.sh \
    scripts/quality/verify-docs-structure.sh \
    scripts/quality/verify-evidence-structure.sh; do
    [[ -x "$path" ]] || fail "required verification script is not executable: $path"
done

echo '== repository scope =='
git diff --check
git diff --cached --check

echo '== inherited Phase 10 verification =='
bash scripts/phase10/verify.sh

echo '== Phase 11 release baseline =='
bash scripts/phase11/verify-release-scope.sh

echo '== Phase 11 final system acceptance =='
bash scripts/phase11/verify-final-acceptance.sh

echo '== Phase 11 documentation and evidence closure =='
bash scripts/quality/verify-docs-structure.sh
bash scripts/quality/verify-evidence-structure.sh

echo '== Phase 11 completed-status contract =='
grep -Fq '**Completed** — release identifier `v1.0.0`.' docs/phase11/README.md || \
    fail 'docs/phase11/README.md is not in completed state'
grep -Fq '**Completed** — release identifier `v1.0.0`.' docs/architecture/system-overview.md || \
    fail 'docs/architecture/system-overview.md is not in completed state'
grep -Fq 'Completed. Phase 11 established the final engineering-release workflow and the `v1.0.0` milestone.' \
    evidence/phase11/README.md || fail 'Phase 11 evidence status is not completed'

stale_file="$(mktemp)"
trap 'rm -f "$stale_file"' EXIT
if grep -R -nE 'In progress|target milestone|PENDING|NOT STARTED' \
    README.md \
    ROADMAP.md \
    docs/phase11 \
    docs/architecture/system-overview.md \
    docs/interview/phase11-questions.md \
    evidence/phase11 >"$stale_file" 2>/dev/null; then
    cat "$stale_file" >&2
    fail 'stale Phase 11 current-facing status remains'
fi

echo 'Phase 11 automated verification: PASS'
