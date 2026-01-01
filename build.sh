#!/bin/bash
# Build script for Luanti with quad-sphere planet support

set -e

BUILD_DIR="build"
BUILD_TYPE="${1:-Release}"
# Detect number of CPU cores (works on both macOS and Linux)
if [[ "$OSTYPE" == "darwin"* ]]; then
    DEFAULT_JOBS=$(sysctl -n hw.ncpu)
else
    DEFAULT_JOBS=$(nproc)
fi
JOBS="${2:-$DEFAULT_JOBS}"

echo "=== Luanti Planet Build ==="
echo "Build type: $BUILD_TYPE"
echo "Parallel jobs: $JOBS"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DENABLE_GETTEXT=ON \
    -DENABLE_FREETYPE=ON \
    -DENABLE_SOUND=ON

# Build
cmake --build . -j "$JOBS"

echo ""
echo "=== Build Complete ==="
echo "Binary location: $BUILD_DIR/bin/luanti"
echo ""
echo "To enable planet mode for a world, add to world's map_meta.txt:"
echo "  planet_mode = true"
echo "  planet_radius = 640"
echo "  planet_face_blocks = 128"
echo "  planet_max_altitude = 32"
echo "  planet_min_altitude = -64"
echo "  planet_sea_level = 0"
