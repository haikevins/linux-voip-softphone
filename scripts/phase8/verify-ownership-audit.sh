#!/usr/bin/env bash
# Read-only Phase 8 Commit 7 Qt/PJSIP ownership audit inventory.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT"

test "$(git branch --show-current)" = "phase/8-multithreading" || {
    echo "ABORT: expected phase/8-multithreading" >&2
    exit 1
}
test "$(git rev-parse --short=7 HEAD)" = "ae870ff" || {
    echo "ABORT: expected Phase 8 Commit 6 at ae870ff" >&2
    exit 1
}
# Audit should not be used as a substitute for reviewing modified production source.
git diff --quiet HEAD -- src include app apps gui qml || {
    echo "ABORT: app source has uncommitted changes; review before auditing" >&2
    exit 1
}

if test -n "$(git ls-files --others --exclude-standard -- src include app apps gui qml)"; then
    echo "ABORT: untracked app source present; review before auditing" >&2
    exit 1
fi
if (( $# != 0 )); then
    echo "ABORT: wrapper accepts no arguments; rerun Python locator separately" >&2
    exit 1
fi

echo "===== AUDIT LOCATOR UNIT TESTS ====="
python3 -m unittest discover -s tests/phase8 -p test_ownership_audit.py -v

echo "===== QT/PJSIP OWNER HOTSPOT INVENTORY ====="
python3 scripts/phase8/audit-qt-pjsip-ownership.py --max-hits 200
echo "===== AUDIT INVENTORY COMPLETE (VERDICT: INCONCLUSIVE) ====="
