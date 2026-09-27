#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

NETWORK_TEST_REGEX='^(Ipv4EndpointTest|TcpClientSocketTest|TcpServerSocketTest|UdpSocketTest|TcpNetworkIntegrationTest|UdpNetworkIntegrationTest)\.'

echo "== repository scope =="
git diff --check

echo "== clang-format =="
find src tests labs -type f \( -name '*.cpp' -o -name '*.hpp' \) \
    -print0 | xargs -0 clang-format --dry-run --Werror

echo "== debug build =="
cmake --preset debug
cmake --build --preset debug -j

echo "== Phase 3 focused networking tests =="
ctest --preset debug -R "${NETWORK_TEST_REGEX}" --output-on-failure

echo "== full regression =="
ctest --preset debug --output-on-failure

echo "== sanitizers =="
./scripts/run_sanitizers.sh

echo "== clang-tidy =="
rm -rf build/clang-tidy
./scripts/run_clang_tidy.sh

echo "Phase 3 verification passed."
