#!/usr/bin/env bash
set -euo pipefail

sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    gdb \
    clang \
    clang-format \
    clang-tidy \
    pkg-config \
    libgtest-dev \
    qt6-base-dev \
    qt6-declarative-dev \
    qml6-module-qtquick \
    ca-certificates

echo
echo "Project toolchain versions:"
git --version
cmake --version | head -n 1
c++ --version | head -n 1
clang --version | head -n 1
clang-format --version
clang-tidy --version | head -n 1
