// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file TestSZ3DualVersion.cxx
 * @brief Test dual-version SZ3 support (v3.3.0 and v3.3.2)
 *
 * This test verifies:
 * 1. Version detection from compressed data header
 * 2. Compression with v3.3.2 (current default)
 * 3. Decompression using version-compatible wrapper
 * 4. Direct decompression with v3.3.0 API
 */

#include "vtk_sz3.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

// Test data generation
static std::vector<float> generateTestData(size_t dimX, size_t dimY, size_t dimZ)
{
  size_t numElements = dimX * dimY * dimZ;
  std::vector<float> data(numElements);
  for (size_t k = 0; k < dimZ; ++k)
  {
    for (size_t j = 0; j < dimY; ++j)
    {
      for (size_t i = 0; i < dimX; ++i)
      {
        size_t idx = k * dimY * dimX + j * dimX + i;
        data[idx] = static_cast<float>(
          std::sin(i * 0.2) * std::cos(j * 0.2) * std::sin(k * 0.2) * 50.0);
      }
    }
  }
  return data;
}

// Test 1: Version detection
static bool testVersionDetection()
{
  std::cout << "\n=== Test 1: Version Detection ===" << std::endl;

  // Create a mock header with v3.3.2 version
  // Header format: [magic(4)] [version(4)] [size(8)]
  char mockHeader332[16];
  uint32_t magic = 0xF342F310;
  uint32_t ver332 = (3 << 24) | (3 << 16) | (2 << 8);  // 3.3.2
  uint64_t size = 0;
  std::memcpy(mockHeader332, &magic, 4);
  std::memcpy(mockHeader332 + 4, &ver332, 4);
  std::memcpy(mockHeader332 + 8, &size, 8);

  std::string detected = vtk_sz3::detect_version(mockHeader332, 16);
  std::cout << "Mock v3.3.2 header detected as: " << detected << std::endl;
  if (detected != "3.3.2")
  {
    std::cerr << "FAIL: Expected '3.3.2' but got '" << detected << "'" << std::endl;
    return false;
  }

  // Create a mock header with v3.3.0 version
  char mockHeader330[16];
  uint32_t ver330 = (3 << 24) | (3 << 16) | (0 << 8);  // 3.3.0
  std::memcpy(mockHeader330, &magic, 4);
  std::memcpy(mockHeader330 + 4, &ver330, 4);
  std::memcpy(mockHeader330 + 8, &size, 8);

  detected = vtk_sz3::detect_version(mockHeader330, 16);
  std::cout << "Mock v3.3.0 header detected as: " << detected << std::endl;
  if (detected != "3.3.0")
  {
    std::cerr << "FAIL: Expected '3.3.0' but got '" << detected << "'" << std::endl;
    return false;
  }

  // Test invalid magic number
  char invalidHeader[16] = {0};
  detected = vtk_sz3::detect_version(invalidHeader, 16);
  if (!detected.empty())
  {
    std::cerr << "FAIL: Expected empty string for invalid magic, got '" << detected << "'" << std::endl;
    return false;
  }
  std::cout << "Invalid header correctly detected as non-SZ3" << std::endl;

  std::cout << "PASS: Version detection works correctly" << std::endl;
  return true;
}

// Test 2: Compress with v3.3.2 and decompress with compat wrapper
static bool testCompressDecompressCompat()
{
  std::cout << "\n=== Test 2: Compress/Decompress with Compat Wrapper ===" << std::endl;

  const size_t dimX = 16, dimY = 16, dimZ = 16;
  auto originalData = generateTestData(dimX, dimY, dimZ);
  size_t numElements = originalData.size();

  // Compress using compat wrapper (uses v3.3.2)
  SZ3::Config conf(dimX, dimY, dimZ);
  conf.cmprAlgo = SZ3::ALGO_INTERP_LORENZO;
  conf.errorBoundMode = SZ3::EB_ABS;
  conf.absErrorBound = 1e-4;

  size_t compressedSize = 0;
  char* compressedData = vtk_sz3::SZ_compress_compat(conf, originalData.data(), compressedSize);

  if (compressedData == nullptr || compressedSize == 0)
  {
    std::cerr << "FAIL: Compression failed" << std::endl;
    return false;
  }

  std::cout << "Compressed " << numElements * sizeof(float) << " bytes to "
            << compressedSize << " bytes (ratio: "
            << static_cast<double>(numElements * sizeof(float)) / compressedSize << ")" << std::endl;

  // Verify version in compressed data
  std::string version = vtk_sz3::detect_version(compressedData, compressedSize);
  std::cout << "Compressed data version: " << version << std::endl;
  if (version != "3.3.2")
  {
    std::cerr << "FAIL: Expected compressed data to be v3.3.2" << std::endl;
    delete[] compressedData;
    return false;
  }

  // Decompress using compat wrapper
  float* decompressedData = nullptr;
  try
  {
    vtk_sz3::SZ_decompress_compat(compressedData, compressedSize, decompressedData);
  }
  catch (const std::exception& e)
  {
    std::cerr << "FAIL: Decompression threw exception: " << e.what() << std::endl;
    delete[] compressedData;
    return false;
  }

  if (decompressedData == nullptr)
  {
    std::cerr << "FAIL: Decompression returned null" << std::endl;
    delete[] compressedData;
    return false;
  }

  // Verify data integrity
  double maxError = 0.0;
  for (size_t i = 0; i < numElements; ++i)
  {
    double error = std::fabs(originalData[i] - decompressedData[i]);
    if (error > maxError) maxError = error;
  }

  std::cout << "Max reconstruction error: " << maxError
            << " (bound: " << conf.absErrorBound << ")" << std::endl;

  delete[] compressedData;
  delete[] decompressedData;

  if (maxError > conf.absErrorBound * 1.01)
  {
    std::cerr << "FAIL: Error exceeds bound" << std::endl;
    return false;
  }

  std::cout << "PASS: Compat wrapper compress/decompress works" << std::endl;
  return true;
}

// Test 3: Direct v3.3.0 API compression/decompression
static bool testV330DirectAPI()
{
  std::cout << "\n=== Test 3: Direct v3.3.0 API ===" << std::endl;

  const size_t dimX = 16, dimY = 16, dimZ = 16;
  auto originalData = generateTestData(dimX, dimY, dimZ);
  size_t numElements = originalData.size();

  // Compress using v3.3.0 API directly
  SZ3_v330::Config conf_v330(dimX, dimY, dimZ);
  conf_v330.cmprAlgo = SZ3_v330::ALGO_INTERP_LORENZO;
  conf_v330.errorBoundMode = SZ3_v330::EB_ABS;
  conf_v330.absErrorBound = 1e-4;

  size_t compressedSize = 0;
  char* compressedData = SZ_compress_v330(conf_v330, originalData.data(), compressedSize);

  if (compressedData == nullptr || compressedSize == 0)
  {
    std::cerr << "FAIL: v3.3.0 compression failed" << std::endl;
    return false;
  }

  std::cout << "v3.3.0 compressed " << numElements * sizeof(float) << " bytes to "
            << compressedSize << " bytes" << std::endl;

  // Verify version in compressed data
  std::string version = vtk_sz3::detect_version(compressedData, compressedSize);
  std::cout << "Compressed data version: " << version << std::endl;
  if (version != "3.3.0")
  {
    std::cerr << "FAIL: Expected v3.3.0 compressed data" << std::endl;
    delete[] compressedData;
    return false;
  }

  // Decompress using v3.3.0 API directly
  SZ3_v330::Config decompConf_v330;
  float* decompressedData = SZ_decompress_v330<float>(decompConf_v330, compressedData, compressedSize);

  if (decompressedData == nullptr)
  {
    std::cerr << "FAIL: v3.3.0 decompression failed" << std::endl;
    delete[] compressedData;
    return false;
  }

  // Verify data integrity
  double maxError = 0.0;
  for (size_t i = 0; i < numElements; ++i)
  {
    double error = std::fabs(originalData[i] - decompressedData[i]);
    if (error > maxError) maxError = error;
  }

  std::cout << "Max reconstruction error: " << maxError << std::endl;

  delete[] compressedData;
  delete[] decompressedData;

  if (maxError > conf_v330.absErrorBound * 1.01)
  {
    std::cerr << "FAIL: Error exceeds bound" << std::endl;
    return false;
  }

  std::cout << "PASS: Direct v3.3.0 API works" << std::endl;
  return true;
}

// Test 4: Cross-version compatibility (compress with v3.3.0, decompress with compat)
static bool testCrossVersionCompat()
{
  std::cout << "\n=== Test 4: Cross-Version Compatibility ===" << std::endl;

  const size_t dimX = 16, dimY = 16, dimZ = 16;
  auto originalData = generateTestData(dimX, dimY, dimZ);
  size_t numElements = originalData.size();

  // Compress using v3.3.0 API
  SZ3_v330::Config conf_v330(dimX, dimY, dimZ);
  conf_v330.cmprAlgo = SZ3_v330::ALGO_INTERP_LORENZO;
  conf_v330.errorBoundMode = SZ3_v330::EB_ABS;
  conf_v330.absErrorBound = 1e-4;

  size_t compressedSize = 0;
  char* compressedData = SZ_compress_v330(conf_v330, originalData.data(), compressedSize);

  if (compressedData == nullptr)
  {
    std::cerr << "FAIL: v3.3.0 compression failed" << std::endl;
    return false;
  }

  std::cout << "Compressed with v3.3.0, attempting to decompress with compat wrapper..." << std::endl;

  // Decompress using compat wrapper (should auto-detect v3.3.0)
  float* decompressedData = nullptr;
  try
  {
    vtk_sz3::SZ_decompress_compat(compressedData, compressedSize, decompressedData);
  }
  catch (const std::exception& e)
  {
    std::cerr << "FAIL: Compat decompression threw exception: " << e.what() << std::endl;
    delete[] compressedData;
    return false;
  }

  if (decompressedData == nullptr)
  {
    std::cerr << "FAIL: Compat decompression returned null" << std::endl;
    delete[] compressedData;
    return false;
  }

  // Verify data integrity
  double maxError = 0.0;
  for (size_t i = 0; i < numElements; ++i)
  {
    double error = std::fabs(originalData[i] - decompressedData[i]);
    if (error > maxError) maxError = error;
  }

  std::cout << "Max reconstruction error: " << maxError << std::endl;

  delete[] compressedData;
  delete[] decompressedData;

  if (maxError > conf_v330.absErrorBound * 1.01)
  {
    std::cerr << "FAIL: Error exceeds bound" << std::endl;
    return false;
  }

  std::cout << "PASS: Cross-version decompression works" << std::endl;
  return true;
}

int TestSZ3DualVersion(int /*argc*/, char* /*argv*/[])
{
  std::cout << "Testing SZ3 Dual-Version Support (v3.3.0 + v3.3.2)" << std::endl;
  std::cout << "=================================================" << std::endl;

  bool allPassed = true;

  allPassed &= testVersionDetection();
  allPassed &= testCompressDecompressCompat();
  allPassed &= testV330DirectAPI();
  allPassed &= testCrossVersionCompat();

  std::cout << "\n=================================================" << std::endl;
  if (allPassed)
  {
    std::cout << "All tests PASSED" << std::endl;
    return EXIT_SUCCESS;
  }
  else
  {
    std::cout << "Some tests FAILED" << std::endl;
    return EXIT_FAILURE;
  }
}
