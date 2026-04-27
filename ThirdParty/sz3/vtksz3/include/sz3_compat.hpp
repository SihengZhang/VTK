/**
 * @file sz3_compat.hpp
 * @brief Version-compatible SZ3 API wrapper for VTK
 *
 * This header provides a version-dispatching API that can decompress data
 * compressed with either SZ3 v3.3.0 or v3.3.2. It detects the version from
 * the compressed data header and calls the appropriate decompressor.
 */

#ifndef VTK_SZ3_COMPAT_HPP
#define VTK_SZ3_COMPAT_HPP

#include "SZ3/api/sz.hpp"         // v3.3.2
#include "SZ3_v330/api/sz.hpp"    // v3.3.0 (namespace mangled)

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace vtk_sz3 {

/**
 * @brief Detect SZ3 version from compressed data header
 *
 * The compressed data header format:
 * - Bytes 0-3: Magic number (0xF342F310)
 * - Bytes 4-7: Version encoded as (major << 24) | (minor << 16) | (patch << 8)
 *
 * @param cmpData Pointer to compressed data
 * @param cmpSize Size of compressed data
 * @return Version string (e.g., "3.3.2", "3.3.0") or empty string if invalid
 */
inline std::string detect_version(const char* cmpData, size_t cmpSize) {
    if (cmpSize < 8) {
        return "";
    }

    uint32_t magic = 0;
    uint32_t versionInt = 0;
    std::memcpy(&magic, cmpData, sizeof(uint32_t));
    std::memcpy(&versionInt, cmpData + 4, sizeof(uint32_t));

    // Check magic number (SZ3 uses 0xF342F310)
    if (magic != 0xF342F310) {
        return "";  // Not SZ3 compressed data
    }

    // Decode version: (major << 24) | (minor << 16) | (patch << 8)
    uint32_t major = (versionInt >> 24) & 0xFF;
    uint32_t minor = (versionInt >> 16) & 0xFF;
    uint32_t patch = (versionInt >> 8) & 0xFF;

    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

/**
 * @brief Version-dispatching decompression (with output config)
 *
 * Automatically detects the SZ3 version from compressed data header and uses
 * the appropriate decompressor. Returns the config that was embedded in the
 * compressed data.
 *
 * @tparam T Data type (float, double, etc.)
 * @param cmpData Pointer to compressed data
 * @param cmpSize Size of compressed data
 * @param decData Output pointer for decompressed data (allocated if nullptr)
 * @param outConfig Output config from v3.3.2 namespace (for compatibility)
 */
template <class T>
void SZ_decompress_compat(const char* cmpData, size_t cmpSize, T*& decData, SZ3::Config& outConfig) {
    std::string ver = detect_version(cmpData, cmpSize);

    if (ver == "3.3.2") {
        SZ_decompress<T>(outConfig, cmpData, cmpSize, decData);
    } else if (ver == "3.3.0") {
        SZ3_v330::Config conf_v330;
        SZ_decompress_v330<T>(conf_v330, cmpData, cmpSize, decData);
        // Copy basic config info to output (dimensions, etc.)
        outConfig.N = conf_v330.N;
        outConfig.dims = conf_v330.dims;
        outConfig.num = conf_v330.num;
    } else {
        throw std::invalid_argument("Unsupported SZ3 version in compressed data: " + ver);
    }
}

/**
 * @brief Version-dispatching decompression (simple version)
 *
 * Automatically detects the SZ3 version from compressed data header and uses
 * the appropriate decompressor.
 *
 * @tparam T Data type (float, double, etc.)
 * @param cmpData Pointer to compressed data
 * @param cmpSize Size of compressed data
 * @param decData Output pointer for decompressed data (allocated if nullptr)
 */
template <class T>
void SZ_decompress_compat(const char* cmpData, size_t cmpSize, T*& decData) {
    SZ3::Config conf;
    SZ_decompress_compat<T>(cmpData, cmpSize, decData, conf);
}

/**
 * @brief Compression using latest version (v3.3.2)
 *
 * Always compresses using v3.3.2 for forward compatibility.
 *
 * @tparam T Data type (float, double, etc.)
 * @param config Compression configuration
 * @param data Source data pointer
 * @param cmpSize Output parameter for compressed data size
 * @return Pointer to allocated compressed data buffer (caller must delete[])
 */
template <class T>
char* SZ_compress_compat(const SZ3::Config& config, const T* data, size_t& cmpSize) {
    return SZ_compress(config, data, cmpSize);
}

/**
 * @brief Compression to pre-allocated buffer using latest version (v3.3.2)
 *
 * @tparam T Data type (float, double, etc.)
 * @param config Compression configuration
 * @param data Source data pointer
 * @param cmpData Pre-allocated output buffer
 * @param cmpCap Capacity of output buffer
 * @return Actual compressed data size
 */
template <class T>
size_t SZ_compress_compat(const SZ3::Config& config, const T* data, char* cmpData, size_t cmpCap) {
    return SZ_compress(config, data, cmpData, cmpCap);
}

} // namespace vtk_sz3

#endif // VTK_SZ3_COMPAT_HPP
