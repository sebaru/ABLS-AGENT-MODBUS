#!/bin/bash
# build_rpm.sh -- Build and sign RPM via CPack
set -euo pipefail

PACKAGE_ONLY=false
NO_SIGN=false

for arg in "$@"; do
  case "$arg" in
    --package-only|-p)
      PACKAGE_ONLY=true
      ;;
    --no-sign)
      NO_SIGN=true
      ;;
    -h|--help)
      echo "Usage: $0 [--package-only|-p] [--no-sign]"
      exit 0
      ;;
    *)
      echo "Usage: $0 [--package-only|-p] [--no-sign]"
      exit 2
      ;;
  esac
done

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
RPM_SIGNER_ID="6C86F2C11305554A61A2221512671FDB87025D1B"

echo "Building RPM package for abls-agent-modbus..."
echo "Project directory: $PROJECT_DIR"
echo "Build directory:   $BUILD_DIR"
echo "Package-only mode: $PACKAGE_ONLY"
echo "Signing mode:      $([[ "$NO_SIGN" == "true" ]] && echo disabled || echo enabled)"
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

if [[ "$NO_SIGN" == "false" ]]; then
  if ! command -v gpg >/dev/null 2>&1; then
    echo "Error: gpg command not found but signing is enabled"
    exit 1
  fi
  if ! command -v rpmsign >/dev/null 2>&1; then
    echo "Error: rpmsign command not found but signing is enabled"
    exit 1
  fi

  echo "Signing key id:    $RPM_SIGNER_ID"

  rpmsign --key-id "$RPM_SIGNER_ID" --addsign "$runtime_rpm"

  checksig_output="$(rpm --checksig "$runtime_rpm" 2>&1 || true)"
  if ! grep -Eqi 'signatures?[^[:alpha:]]+OK|pgp[^[:alpha:]]+OK|rsa[^[:alpha:]]+OK|ecdsa[^[:alpha:]]+OK|eddsa[^[:alpha:]]+OK' <<<"$checksig_output"; then
    echo "Error: unsigned or invalid signature for $runtime_rpm"
    echo "rpm --checksig output: $checksig_output"
    exit 1
  fi
fi

echo "RPM generated:"
echo "  $runtime_rpm"
echo "  $debuginfo_rpm"
echo "RPM build complete."
