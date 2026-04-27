#!/usr/bin/env bash
# Update and mangle SZ3 v3.3.0 for dual-version support
# This script:
# 1. Downloads SZ3 v3.3.0 from GitHub
# 2. Extracts to SZ3_v330 directory
# 3. Applies namespace mangling to avoid conflicts with v3.3.2

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

readonly TAG="v3.3.0"
readonly REPO_URL="https://github.com/szcompressor/SZ3/archive/refs/tags/${TAG}.tar.gz"
readonly DST="vtksz3/include/SZ3_v330"
readonly TMP_DIR="/tmp/sz3_v330_update_$$"

echo "=== Updating SZ3 v3.3.0 with namespace mangling ==="
echo "Source: ${REPO_URL}"
echo "Destination: ${DST}"

# Download and extract
echo ""
echo "Step 1: Downloading ${TAG}..."
mkdir -p "$TMP_DIR"
curl -L "$REPO_URL" -o "$TMP_DIR/sz3.tar.gz"

echo "Step 2: Extracting..."
tar -xzf "$TMP_DIR/sz3.tar.gz" -C "$TMP_DIR"

# Copy to destination
echo "Step 3: Installing to ${DST}..."
rm -rf "$DST"
cp -r "$TMP_DIR/SZ3-3.3.0/include/SZ3" "$DST"

# Cleanup temp files
rm -rf "$TMP_DIR"

# Apply namespace mangling
echo "Step 4: Applying namespace mangling..."

find "$DST" -name "*.hpp" -o -name "*.hpp.in" | while read -r file; do
    sed -i \
        -e 's/namespace SZ3/namespace SZ3_v330/g' \
        -e 's/SZ3::/SZ3_v330::/g' \
        -e 's/namespace ska/namespace ska_v330/g' \
        -e 's/ska::/ska_v330::/g' \
        -e 's/detailv3/detailv3_v330/g' \
        -e 's/detailv8/detailv8_v330/g' \
        -e 's/detailv10/detailv10_v330/g' \
        -e 's/#define SZ3_/#define SZ3_V330_/g' \
        -e 's/#ifndef SZ3_/#ifndef SZ3_V330_/g' \
        -e 's/#endif.*\/\/ SZ3_/#endif \/\/ SZ3_V330_/g' \
        -e 's/SZ3_MAGIC_NUMBER/SZ3_V330_MAGIC_NUMBER/g' \
        -e 's/SZ3_VER/SZ3_V330_VER/g' \
        -e 's/SZ3_DATA_VER/SZ3_V330_DATA_VER/g' \
        -e 's/SZ3_ERROR_/SZ3_V330_ERROR_/g' \
        -e 's|#include "SZ3/|#include "SZ3_v330/|g' \
        -e 's/SZ_compress</SZ_compress_v330</g' \
        -e 's/SZ_decompress</SZ_decompress_v330</g' \
        -e 's/SZ_compress_size_bound</SZ_compress_size_bound_v330</g' \
        -e 's/SZ_compress(/SZ_compress_v330(/g' \
        -e 's/SZ_decompress(/SZ_decompress_v330(/g' \
        -e 's/SZ_compress_size_bound(/SZ_compress_size_bound_v330(/g' \
        -e 's/versionInt(/versionInt_v330(/g' \
        -e 's/versionStr(/versionStr_v330(/g' \
        "$file"
done

# Fix version.hpp.in CMake variables
if [ -f "$DST/version.hpp.in" ]; then
    echo "Step 5: Updating version.hpp.in CMake variables..."
    sed -i \
        -e 's/@PROJECT_NAME@/@SZ3_V330_PROJECT_NAME@/g' \
        -e 's/@PROJECT_VERSION@/@SZ3_V330_VERSION@/g' \
        -e 's/@PROJECT_VERSION_MAJOR@/@SZ3_V330_VERSION_MAJOR@/g' \
        -e 's/@PROJECT_VERSION_MINOR@/@SZ3_V330_VERSION_MINOR@/g' \
        -e 's/@PROJECT_VERSION_PATCH@/@SZ3_V330_VERSION_PATCH@/g' \
        -e 's/@PROJECT_VERSION_TWEAK@/@SZ3_V330_VERSION_TWEAK@/g' \
        -e 's/@SZ3_DATA_VERSION@/@SZ3_V330_DATA_VERSION@/g' \
        "$DST/version.hpp.in"
fi

echo ""
echo "=== Update complete ==="
echo "Files transformed: $(find "$DST" -name "*.hpp" | wc -l) .hpp files"
echo ""
echo "To rebuild VTK, run cmake --build in your build directory."
