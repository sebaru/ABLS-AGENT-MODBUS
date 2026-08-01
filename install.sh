#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")" && pwd)"
build_dir="${project_dir}/build"

if [[ ! -d "${build_dir}" ]]; then
  echo "Build directory missing: ${build_dir}" >&2
  exit 1
fi

cd "${build_dir}"
sudo cmake --install .
