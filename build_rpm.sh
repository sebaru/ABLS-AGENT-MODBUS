#!/bin/bash
# build_rpm.sh -- Build RPM via CPack
set -euo pipefail

PACKAGE_ONLY=false

for arg in "$@"; do
  case "$arg" in
    --package-only|-p)
      PACKAGE_ONLY=true
      ;;
    -h|--help)
      echo "Usage: $0 [--package-only|-p]"
      exit 0
      ;;
    *)
      echo "Usage: $0 [--package-only|-p]"
      exit 2
      ;;
  esac
done

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "Building RPM package for abls-agent-modbus..."
echo "Project directory: $PROJECT_DIR"
echo "Build directory:   $BUILD_DIR"
echo "Package-only mode: $PACKAGE_ONLY"
echo "Signing mode:      disabled (centralized in ABLS-PKGS)"
echo "Install prefix:    /usr (forced for RPM packaging)"

mkdir -p "$BUILD_DIR"

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo

if [[ "$PACKAGE_ONLY" == "false" ]]; then
  cmake --build "$BUILD_DIR" -- -j"$(nproc)"
elif [[ ! -f "$BUILD_DIR/CPackConfig.cmake" ]]; then
  echo "Missing $BUILD_DIR/CPackConfig.cmake. Run ./build.sh first or use full mode."
  exit 1
fi

rm -f "$BUILD_DIR"/abls-agent-modbus-*.rpm

pushd "$BUILD_DIR" >/dev/null
cpack -G RPM
popd >/dev/null

runtime_rpm=$(find "$BUILD_DIR" -maxdepth 1 -type f -name 'abls-agent-modbus-[0-9]*.rpm' | sort | tail -n 1)
debuginfo_rpm=$(find "$BUILD_DIR" -maxdepth 1 -type f -name '*debuginfo*.rpm' | sort | tail -n 1)

if [[ -z "$runtime_rpm" ]]; then
  echo "RPM generation failed: expected runtime package in $BUILD_DIR"
  exit 1
fi

if [[ -z "$debuginfo_rpm" ]]; then
  echo "RPM generation failed: expected debuginfo package in $BUILD_DIR"
  exit 1
fi

publish_to_abls_pkgs_repo() {
  local target_repo_root="${ABLS_PKGS_REPO_DIR:-$PROJECT_DIR/../ABLS-PKGS}"
  local resolved_public_dir=""

  if [[ -d "$target_repo_root/public" ]]; then
    resolved_public_dir="$target_repo_root/public"
  elif [[ -d "$target_repo_root/rpms" || "$(basename "$target_repo_root")" == "public" ]]; then
    resolved_public_dir="$target_repo_root"
  else
    resolved_public_dir="$target_repo_root/public"
  fi

  if [[ ! -d "$resolved_public_dir" ]]; then
    echo "WARN: ABLS-PKGS repo not found at $resolved_public_dir; skipping publish"
    return 0
  fi

  shopt -s nullglob
  local rpm_file
  for rpm_file in "$BUILD_DIR"/*.rpm; do
    local arch target_dir
    arch="$(rpm -qp --qf '%{ARCH}' "$rpm_file" 2>/dev/null || true)"
    target_dir="$resolved_public_dir/rpms"
    [[ -n "$arch" ]] && target_dir="$target_dir/$arch"
    mkdir -p "$target_dir"
    cp -f "$rpm_file" "$target_dir/"
  done
  shopt -u nullglob

  echo "Published to:"
  echo "  $resolved_public_dir/rpms"
}

publish_to_abls_pkgs_repo

echo "RPM generated:"
echo "  $runtime_rpm"
echo "  $debuginfo_rpm"
echo "RPM build complete."
