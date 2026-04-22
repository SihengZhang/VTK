// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtk_sz3.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

int TestCompressSZ3(int /*argc*/, char* /*argv*/[])
{
  // Create test data: a 3D array with smooth variation
  const size_t dimX = 32;
  const size_t dimY = 32;
  const size_t dimZ = 32;
  const size_t numElements = dimX * dimY * dimZ;

  std::vector<float> originalData(numElements);
  for (size_t k = 0; k < dimZ; ++k)
  {
    for (size_t j = 0; j < dimY; ++j)
    {
      for (size_t i = 0; i < dimX; ++i)
      {
        size_t idx = k * dimY * dimX + j * dimX + i;
        // Smooth function for good compression
        originalData[idx] = static_cast<float>(
          std::sin(i * 0.1) * std::cos(j * 0.1) * std::sin(k * 0.1) * 100.0);
      }
    }
  }

  // Configure SZ3 compression
  SZ3::Config conf(dimX, dimY, dimZ);
  conf.cmprAlgo = SZ3::ALGO_INTERP_LORENZO;
  conf.errorBoundMode = SZ3::EB_ABS;
  conf.absErrorBound = 1e-3;

  // Compress
  size_t compressedSize = 0;
  char* compressedData = SZ_compress(conf, originalData.data(), compressedSize);

  if (compressedData == nullptr || compressedSize == 0)
  {
    std::cerr << "SZ3 compression failed!" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Original size: " << numElements * sizeof(float) << " bytes" << std::endl;
  std::cout << "Compressed size: " << compressedSize << " bytes" << std::endl;
  std::cout << "Compression ratio: "
            << static_cast<double>(numElements * sizeof(float)) / compressedSize << std::endl;

  // Decompress
  SZ3::Config decompConf;
  float* decompressedData = SZ_decompress<float>(decompConf, compressedData, compressedSize);

  if (decompressedData == nullptr)
  {
    std::cerr << "SZ3 decompression failed!" << std::endl;
    delete[] compressedData;
    return EXIT_FAILURE;
  }

  // Verify decompressed data is within error bound
  double maxError = 0.0;
  for (size_t i = 0; i < numElements; ++i)
  {
    double error = std::fabs(originalData[i] - decompressedData[i]);
    if (error > maxError)
    {
      maxError = error;
    }
  }

  std::cout << "Max reconstruction error: " << maxError << std::endl;
  std::cout << "Error bound: " << conf.absErrorBound << std::endl;

  // Clean up
  delete[] compressedData;
  delete[] decompressedData;

  // Check that error is within tolerance (allow small floating point margin)
  if (maxError > conf.absErrorBound * 1.01)
  {
    std::cerr << "Reconstruction error exceeds error bound!" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "SZ3 compression/decompression test passed." << std::endl;
  return EXIT_SUCCESS;
}
