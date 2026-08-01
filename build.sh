#!/bin/bash

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

if [ "${1:-}" = "clean" ]; then
    rm -rf "$BUILD_DIR"
fi

echo "Building ABLS Agent Modbus..."
echo "Project directory: $PROJECT_DIR"
echo "Build directory: $BUILD_DIR"
echo "Number of processors: $(nproc)"

if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -- -j$(nproc)

echo ""
echo "Build completed successfully"
echo "Built artifacts are in: $BUILD_DIR"
