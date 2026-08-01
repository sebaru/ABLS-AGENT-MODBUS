#!/bin/bash
# build_apt.sh -- Build DEB package for abls-agent-modbus via CPack
set -euo pipefail

PACKAGE_ONLY=false
NO_SIGN=false
CLEAN=false
TARGET_DIST="bookworm"
TARGET_ARCH=""

usage() {
  cat <<'EOF'
Usage: ./build_apt.sh [options]

Options:
  --package-only, -p   Skip compilation and only run cpack
  --no-sign            Skip package signing
  --clean              Remove old .deb artifacts before build
  --dist <suite>       Target suite label for output path (default: bookworm)
  --arch <arch>        Target Debian arch (default: host arch)
  -h, --help           Show this help

Notes:
- This script uses native build toolchain by default.
- --dist is used to organize output artifacts only.
- To sign packages, export DEB_SIGNER_ID and install dpkg-sig.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --package-only|-p)
      PACKAGE_ONLY=true
      shift
      ;;
    --no-sign)
      NO_SIGN=true
      shift
      ;;
    --clean)
      CLEAN=true
      shift
      ;;
    --dist)
      TARGET_DIST="${2:-}"
      [[ -n "$TARGET_DIST" ]] || { echo "Missing value for --dist"; exit 2; }
      shift 2
      ;;
    --arch)
      TARGET_ARCH="${2:-}"
      [[ -n "$TARGET_ARCH" ]] || { echo "Missing value for --arch"; exit 2; }
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

if [[ -z "$TARGET_ARCH" ]]; then
  if command -v dpkg >/dev/null 2>&1; then
    TARGET_ARCH="$(dpkg --print-architecture)"
  else
    echo "Error: dpkg not found. Install Debian packaging tools first."
    exit 1
  fi
fi

ARTIFACT_DIR="$BUILD_DIR/deb/$TARGET_DIST/$TARGET_ARCH"

echo "Building DEB package for abls-agent-modbus..."
echo "Project directory: $PROJECT_DIR"
echo "Build directory:   $BUILD_DIR"
echo "Output directory:  $ARTIFACT_DIR"
echo "Package-only mode: $PACKAGE_ONLY"
echo "Signing mode:      $([[ "$NO_SIGN" == "true" ]] && echo disabled || echo enabled)"
echo "Target suite:      $TARGET_DIST"
echo "Target arch:       $TARGET_ARCH"

mkdir -p "$BUILD_DIR"
mkdir -p "$ARTIFACT_DIR"

if [[ "$CLEAN" == "true" ]]; then
  rm -f "$BUILD_DIR"/abls-agent-modbus*.deb
  rm -f "$ARTIFACT_DIR"/abls-agent-modbus*.deb
fi

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCPACK_DEBIAN_PACKAGE_ARCHITECTURE="$TARGET_ARCH"

if [[ "$PACKAGE_ONLY" == "false" ]]; then
  cmake --build "$BUILD_DIR" -- -j"$(nproc)"
elif [[ ! -f "$BUILD_DIR/CPackConfig.cmake" ]]; then
  echo "Missing $BUILD_DIR/CPackConfig.cmake. Run full mode first."
  exit 1
fi

pushd "$BUILD_DIR" >/dev/null
cpack -G DEB
popd >/dev/null

runtime_deb=""

while IFS= read -r deb_file; do
  if command -v dpkg-deb >/dev/null 2>&1; then
    pkg_name="$(dpkg-deb -f "$deb_file" Package 2>/dev/null || true)"
    if [[ "$pkg_name" == "abls-agent-modbus" ]]; then
      runtime_deb="$deb_file"
      continue
    fi
  fi

  case "$(basename "$deb_file")" in
    *runtime*.deb|abls-agent-modbus-*.deb)
      runtime_deb="$deb_file"
      ;;
  esac
done < <(find "$BUILD_DIR" -maxdepth 1 -type f -name '*.deb' | sort)

if [[ -z "$runtime_deb" ]]; then
  echo "DEB generation failed: expected runtime package in $BUILD_DIR"
  exit 1
fi

if [[ "$NO_SIGN" == "false" ]]; then
  if command -v dpkg-sig >/dev/null 2>&1; then
    if [[ -z "${DEB_SIGNER_ID:-}" ]]; then
      echo "Error: DEB_SIGNER_ID is required for signing"
      exit 1
    fi
    dpkg-sig --sign builder -k "$DEB_SIGNER_ID" "$runtime_deb"
  else
    echo "Error: dpkg-sig command not found but signing is enabled"
    exit 1
  fi
fi

copy_with_normalized_name() {
  local src_file="$1"
  local dst_dir="$2"

  if command -v dpkg-deb >/dev/null 2>&1; then
    local pkg version arch
    pkg="$(dpkg-deb -f "$src_file" Package 2>/dev/null || true)"
    version="$(dpkg-deb -f "$src_file" Version 2>/dev/null || true)"
    arch="$(dpkg-deb -f "$src_file" Architecture 2>/dev/null || true)"
    if [[ -n "$pkg" && -n "$version" && -n "$arch" ]]; then
      cp -f "$src_file" "$dst_dir/${pkg}_${version}_${arch}.deb"
      return
    fi
  fi

  cp -f "$src_file" "$dst_dir/"
}

copy_with_normalized_name "$runtime_deb" "$ARTIFACT_DIR"

echo "DEB generated:"
echo "  $runtime_deb"
echo "Copied to:"
echo "  $ARTIFACT_DIR"
echo "DEB build complete."
