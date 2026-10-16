#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

echo '== repository scope =='
[[ -f CMakeLists.txt ]]
[[ -f CMakePresets.json ]]
[[ -x scripts/phase10/verify.sh ]]
[[ -x scripts/phase11/verify-release-scope.sh ]]
[[ -x scripts/phase11/verify-package.sh ]]

echo '== inherited Phase 10 verification =='
bash scripts/phase10/verify.sh

echo '== Phase 11 release baseline =='
bash scripts/phase11/verify-release-scope.sh

echo '== Phase 11 release build and package =='
bash scripts/phase11/verify-package.sh

echo 'Phase 11 Commit 2 verification: PASS'
