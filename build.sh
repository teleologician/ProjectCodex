#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export VCPKG_ROOT="${VCPKG_ROOT:-$ROOT_DIR/vcpkg}"

cmake --preset release
cmake --build --preset release
"$ROOT_DIR/bin/Release/vampire.exe"
