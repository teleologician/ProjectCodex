#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export VCPKG_ROOT="${VCPKG_ROOT:-$ROOT_DIR/vcpkg}"

cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build "$ROOT_DIR/build" --config Release
"$ROOT_DIR/build/Release/hello.exe"
