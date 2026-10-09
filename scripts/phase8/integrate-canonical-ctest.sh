#!/usr/bin/env bash
set -euo pipefail

# Deliberately modifies only the canonical tests/CMakeLists.txt, since that
# file is not included in assistant overlay ZIPs without a verified baseline.
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

command -v git >/dev/null || { echo 'ABORT: git required' >&2; exit 1; }
test "$(git rev-parse --show-toplevel)" = "$REPO_ROOT" || {
    echo 'ABORT: unexpected Git root' >&2; exit 1;
}
test "$(git branch --show-current)" = 'phase/8-multithreading' || {
    echo 'ABORT: incorrect Git branch' >&2; exit 1;
}
test "$(git rev-parse --short=7 HEAD)" = '9358f8b' || {
    echo 'ABORT: expected verified Commit 3 baseline 9358f8b' >&2; exit 1;
}
test -f tests/CMakeLists.txt && test ! -L tests/CMakeLists.txt || {
    echo 'ABORT: tests/CMakeLists.txt missing or a symlink' >&2; exit 1;
}
git ls-files --error-unmatch tests/CMakeLists.txt >/dev/null || {
    echo 'ABORT: tests/CMakeLists.txt is not tracked' >&2; exit 1;
}
git diff --quiet -- tests/CMakeLists.txt &&
    git diff --cached --quiet -- tests/CMakeLists.txt || {
        echo 'ABORT: tests/CMakeLists.txt has existing edits' >&2; exit 1;
    }
test -f tests/phase8/CMakeLists.txt &&
    test -f tests/phase8/test_thread_ownership.cpp || {
        echo 'ABORT: Commit 4 overlay files are missing' >&2; exit 1;
    }

python3 - <<'PY'
from pathlib import Path
import sys

path = Path('tests/CMakeLists.txt')
original = path.read_text(encoding='utf-8')
marker = '# Phase 8 concurrency CTest registration'
line = 'add_subdirectory(phase8)'
if marker in original or line in original:
    sys.exit('ABORT: Phase 8 registration is already present; inspect manually')
if len(original.strip()) < 80:
    sys.exit('ABORT: unusually short tests/CMakeLists.txt; inspect manually')
if '\x00' in original:
    sys.exit('ABORT: invalid text in tests/CMakeLists.txt')
# Appending is conservative: it retains all original content verbatim and
# adds exactly one child directory registration at the end.
if '\r' in original:
    sys.exit('ABORT: CRLF CMake file; inspect before editing')
separator = '\n' if original.endswith('\n') else '\n\n'
updated = original + separator + marker + '\n' + line + '\n'
path.write_text(updated, encoding='utf-8')
print('PASS: appended Phase 8 registration to tests/CMakeLists.txt')
print('REVIEW REQUIRED: git diff -- tests/CMakeLists.txt')
PY
