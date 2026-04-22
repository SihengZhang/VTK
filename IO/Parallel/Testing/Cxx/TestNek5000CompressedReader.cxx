// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

// Test reading compressed Nek5000 data files using SZ3 decompression.
// This single-process test validates header parsing and format detection.
// For full data reading tests, use TestNek5000CompressedReaderMPI with multiple ranks.

#include "vtkNek5000Reader.h"

#include "vtkInformation.h"
#include "vtkNew.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkUnstructuredGrid.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

int TestNek5000CompressedReader(int argc, char* argv[])
{
  (void)argc;
  (void)argv;

  // Path to compressed data - only run if file exists
  const char* dataFile = "/archives/disk1/viralss2/jicf_comp/cmpjicf0.f00001";

  std::ifstream testFile(dataFile);
  if (!testFile.good())
  {
    std::cout << "Skipping test - compressed data file not found: " << dataFile << std::endl;
    std::cout << "This test requires large compressed Nek5000 data files." << std::endl;
    return EXIT_SUCCESS;
  }
  testFile.close();

  // Create nek5000 index file with single timestep for header parsing test
  const char* compressedPath = "/tmp/test_cmpjicf.nek5000";
  {
    std::ofstream nek5000File(compressedPath);
    nek5000File << " filetemplate: /archives/disk1/viralss2/jicf_comp/cmpjicf%01d.f%05d\n";
    nek5000File << " firsttimestep: 1\n";
    nek5000File << " numtimesteps: 1\n";
    nek5000File.close();
  }

  std::cout << "Testing vtkNek5000Reader header parsing with compressed (#stdc) format" << std::endl;
  std::cout << "NOTE: Full data reading requires MPI. Use TestNek5000CompressedReaderMPI." << std::endl;

  vtkNew<vtkNek5000Reader> reader;
  reader->SetFileName(compressedPath);

  // Just test UpdateInformation - this validates header parsing
  reader->UpdateInformation();

  // Check that information was read correctly
  vtkInformation* outInfo = reader->GetOutputInformation(0);
  if (!outInfo)
  {
    std::cerr << "ERROR: No output information" << std::endl;
    return EXIT_FAILURE;
  }

  // Check timesteps were parsed
  if (outInfo->Has(vtkStreamingDemandDrivenPipeline::TIME_STEPS()))
  {
    int numSteps = outInfo->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    std::cout << "Number of time steps: " << numSteps << std::endl;
    if (numSteps <= 0)
    {
      std::cerr << "ERROR: No time steps found" << std::endl;
      return EXIT_FAILURE;
    }
  }

  // Check available point arrays
  int numArrays = reader->GetNumberOfPointArrays();
  std::cout << "Number of point arrays: " << numArrays << std::endl;
  for (int i = 0; i < numArrays; i++)
  {
    std::cout << "  Array " << i << ": " << reader->GetPointArrayName(i) << std::endl;
  }

  if (numArrays <= 0)
  {
    std::cerr << "ERROR: No point arrays found" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Header parsing test PASSED" << std::endl;
  std::cout << "For full data reading, run: mpirun -np 8 TestNek5000CompressedReaderMPI" << std::endl;
  return EXIT_SUCCESS;
}
