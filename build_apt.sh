#!/bin/bash
# build_apt.sh -- Build DEB package for abls-agent-modbus via CPack
set -euo pipefail

PACKAGE_ONLY=false
CLEAN=false
TARGET_DIST=""
TARGET_ARCH=""
DEB_VERSION_SUFFIX=""
USE_DIST_SUFFIX=true

detect_host_dist() {
  local codename=""

  if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    codename="${VERSION_CODENAME:-${DEBIAN_CODENAME:-}}"
  fi

  if [[ -n "$codename" ]]; then
    printf '%s\n' "$codename"
    return 0
  fi

  printf '%s\n' "bookworm"
}

usage() {
  cat <<'EOF'
Usage: ./build_apt.sh [options]

Options:
  --package-only, -p   Skip compilation and only run cpack
  --clean              Remove old .deb artifacts before build
  --dist <suite>       Target suite label for output path (default: host OS codename)
  --version-suffix <s> Debian version suffix override (example: ~trixie)
  --no-dist-suffix     Disable automatic ~<dist> suffix
  -h, --help           Show this help

Notes:
- This script builds only the native host architecture.
- By default, suite is inferred from /etc/os-release (VERSION_CODENAME/DEBIAN_CODENAME).
- --dist is used for output path and default Debian version suffix (~<dist>).
- Package signing is centralized in ABLS-PKGS.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --package-only|-p)
      PACKAGE_ONLY=true
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
    --version-suffix)
      DEB_VERSION_SUFFIX="${2:-}"
      [[ -n "$DEB_VERSION_SUFFIX" ]] || { echo "Missing value for --version-suffix"; exit 2; }
      shift 2
      ;;
    --no-dist-suffix)
      USE_DIST_SUFFIX=false
      shift
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

if ! command -v dpkg >/dev/null 2>&1; then
  echo "Error: dpkg not found. Install Debian packaging tools first."
  exit 1
fi

host_arch="$(dpkg --print-architecture)"
if [[ -z "$TARGET_ARCH" ]]; then
  TARGET_ARCH="$host_arch"
elif [[ "$TARGET_ARCH" != "$host_arch" ]]; then
  echo "Error: cross compilation disabled. Host arch is '$host_arch', requested '$TARGET_ARCH'." >&2
  exit 1
fi

if [[ -z "$TARGET_DIST" ]]; then
  TARGET_DIST="$(detect_host_dist)"
fi

if [[ -z "$DEB_VERSION_SUFFIX" && "$USE_DIST_SUFFIX" == "true" ]]; then
  DEB_VERSION_SUFFIX="~$TARGET_DIST"
fi

BUILD_DIR="$PROJECT_DIR/build/$TARGET_DIST/$TARGET_ARCH"
ARTIFACT_DIR="$PROJECT_DIR/build/deb/$TARGET_DIST/$TARGET_ARCH"
cmake_args=(
  -DCMAKE_INSTALL_PREFIX=/usr
  -DCPACK_DEBIAN_PACKAGE_ARCHITECTURE="$TARGET_ARCH"
  -DABLS_DEB_VERSION_SUFFIX="$DEB_VERSION_SUFFIX"
)

echo "Building DEB package for abls-agent-modbus..."
echo "Project directory: $PROJECT_DIR"
echo "Build directory:   $BUILD_DIR"
echo "Output directory:  $ARTIFACT_DIR"
echo "Package-only mode: $PACKAGE_ONLY"
echo "Signing mode:      disabled (repository metadata signed in ABLS-PKGS)"
echo "Target suite:      $TARGET_DIST"
echo "Target arch:       $TARGET_ARCH"
echo "Version suffix:    ${DEB_VERSION_SUFFIX:-<none>}"

mkdir -p "$BUILD_DIR"
mkdir -p "$ARTIFACT_DIR"

if [[ "$CLEAN" == "true" ]]; then
  rm -f "$BUILD_DIR"/abls-agent-modbus*.deb
  rm -f "$ARTIFACT_DIR"/abls-agent-modbus*.deb
fi

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" "${cmake_args[@]}"

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

publish_to_abls_pkgs_repo() {
  local target_repo_root="${ABLS_PKGS_REPO_DIR:-$PROJECT_DIR/../ABLS-PKGS}"
  local resolved_repo_root=""

  if [[ -d "$target_repo_root/public" ]]; then
    resolved_repo_root="$target_repo_root"
  elif [[ "$(basename "$target_repo_root")" == "public" ]]; then
    resolved_repo_root="$(cd "$target_repo_root/.." && pwd)"
  else
    resolved_repo_root="$target_repo_root"
  fi

  if [[ ! -d "$resolved_repo_root" ]]; then
    echo "WARN: ABLS-PKGS repo not found at $resolved_repo_root; skipping publish"
    return 0
  fi

  local publish_dir="$resolved_repo_root/deb-packages/$TARGET_DIST/$TARGET_ARCH"
  mkdir -p "$publish_dir"

  shopt -s nullglob
  local deb_file
  for deb_file in "$ARTIFACT_DIR"/*.deb; do
    cp -f "$deb_file" "$publish_dir/"
  done
  shopt -u nullglob

  echo "Published to:"
  echo "  $publish_dir"
}

publish_to_abls_pkgs_repo

echo "DEB generated:"
echo "  $runtime_deb"
echo "Copied to:"
echo "  $ARTIFACT_DIR"
echo "DEB build complete."
