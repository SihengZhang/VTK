// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

// Test that verifies the reader can handle both compressed and uncompressed formats.
// Checks header parsing (time values) and basic data loading.

#include "vtkNek5000Reader.h"

#include "vtkDataArray.h"
#include "vtkInformation.h"
#include "vtkMPIController.h"
#include "vtkNew.h"
#include "vtkPointData.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkUnstructuredGrid.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>

int TestNek5000BothFormats(int argc, char* argv[])
{
  vtkNew<vtkMPIController> controller;
  controller->Initialize(&argc, &argv, 0);
  vtkMultiProcessController::SetGlobalController(controller);

  int myRank = controller->GetLocalProcessId();
  int numRanks = controller->GetNumberOfProcesses();

  bool success = true;

  // Test 1: Compressed format
  if (myRank == 0)
  {
    std::cout << "\n=== Test 1: Compressed Format (#stdc) ===" << std::endl;
  }

  const char* compFile = "/archives/disk1/viralss2/jicf_comp/cmpjicf0.f00001";
  std::ifstream testComp(compFile);
  if (testComp.good())
  {
    testComp.close();

    // Create index file
    if (myRank == 0)
    {
      std::ofstream idx("/tmp/test_comp.nek5000");
      idx << " filetemplate: /archives/disk1/viralss2/jicf_comp/cmpjicf%01d.f%05d\n";
      idx << " firsttimestep: 1\n";
      idx << " numtimesteps: 1\n";
      idx.close();
    }
    controller->Barrier();

    vtkNew<vtkNek5000Reader> reader;
    reader->SetFileName("/tmp/test_comp.nek5000");
    reader->UpdateInformation();

    vtkInformation* info = reader->GetOutputInformation(0);
    int numSteps = info->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    double* times = info->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS());

    if (myRank == 0)
    {
      std::cout << "  Timesteps found: " << numSteps << std::endl;
      if (numSteps > 0)
      {
        std::cout << "  Time value: " << times[0] << std::endl;
        // Compressed files have time=0 in header, but we use synthetic values
        std::cout << "  (Original was 0.0, synthetic value used)" << std::endl;
      }
    }

    // Load data
    reader->EnableAllPointArrays();
    reader->Update();

    vtkUnstructuredGrid* output = reader->GetOutput();
    vtkIdType localPoints = output ? output->GetNumberOfPoints() : 0;
    vtkIdType totalPoints = 0;
    controller->Reduce(&localPoints, &totalPoints, 1, vtkCommunicator::SUM_OP, 0);

    if (myRank == 0)
    {
      std::cout << "  Total points: " << totalPoints << std::endl;
      if (totalPoints == 3240264000LL)
      {
        std::cout << "  COMPRESSED TEST PASSED" << std::endl;
      }
      else
      {
        std::cout << "  COMPRESSED TEST FAILED - wrong point count" << std::endl;
        success = false;
      }
    }
  }
  else if (myRank == 0)
  {
    std::cout << "  Skipping - compressed file not found" << std::endl;
  }

  // Test 2: Uncompressed format (header parsing only due to size)
  if (myRank == 0)
  {
    std::cout << "\n=== Test 2: Uncompressed Format (#std) ===" << std::endl;
  }

  const char* uncFile = "/archives/disk1/viralss2/jicf_comp/uncjicf0.f00001";
  std::ifstream testUnc(uncFile);
  if (testUnc.good())
  {
    testUnc.close();

    // Create index file
    if (myRank == 0)
    {
      std::ofstream idx("/tmp/test_unc.nek5000");
      idx << " filetemplate: /archives/disk1/viralss2/jicf_comp/uncjicf%01d.f%05d\n";
      idx << " firsttimestep: 1\n";
      idx << " numtimesteps: 1\n";
      idx.close();
    }
    controller->Barrier();

    vtkNew<vtkNek5000Reader> reader;
    reader->SetFileName("/tmp/test_unc.nek5000");
    reader->UpdateInformation();

    vtkInformation* info = reader->GetOutputInformation(0);
    int numSteps = info->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    double* times = info->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS());

    if (myRank == 0)
    {
      std::cout << "  Timesteps found: " << numSteps << std::endl;
      if (numSteps > 0)
      {
        std::cout << "  Time value: " << times[0] << std::endl;
        std::cout << "  Expected: ~24.26 (from header 0.2426004779106E+02)" << std::endl;

        // Check time value is correctly parsed (should be ~24.26, not 3240264)
        if (std::abs(times[0] - 24.26) < 1.0)
        {
          std::cout << "  UNCOMPRESSED HEADER TEST PASSED" << std::endl;
        }
        else if (std::abs(times[0] - 3240264.0) < 1.0)
        {
          std::cout << "  UNCOMPRESSED HEADER TEST FAILED - read element count instead of time!" << std::endl;
          success = false;
        }
        else
        {
          std::cout << "  UNCOMPRESSED HEADER TEST FAILED - unexpected time value" << std::endl;
          success = false;
        }
      }
    }

    // Skip full data load for uncompressed (117GB file)
    if (myRank == 0)
    {
      std::cout << "  (Skipping full data load - file is 117GB)" << std::endl;
    }
  }
  else if (myRank == 0)
  {
    std::cout << "  Skipping - uncompressed file not found" << std::endl;
  }

  if (myRank == 0)
  {
    std::cout << "\n=== SUMMARY ===" << std::endl;
    if (success)
    {
      std::cout << "ALL TESTS PASSED" << std::endl;
    }
    else
    {
      std::cout << "SOME TESTS FAILED" << std::endl;
    }
  }

  controller->Finalize();
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
