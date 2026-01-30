#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export VCPKG_ROOT="${VCPKG_ROOT:-$ROOT_DIR/vcpkg}"
export VCPKG_OVERLAY_TRIPLETS="$ROOT_DIR/cmake/triplets"
export VCPKG_DEFAULT_TRIPLET="x64-windows"
export VCPKG_TARGET_TRIPLET="x64-windows"

if [ ! -x "$VCPKG_ROOT/vcpkg.exe" ]; then
  echo "vcpkg.exe not found at $VCPKG_ROOT."
  exit 1
fi
"$VCPKG_ROOT/vcpkg.exe" install --triplet x64-windows
cmake --preset vs2022
cmake --build --preset release
"$ROOT_DIR/bin/Release/vampire.exe"
