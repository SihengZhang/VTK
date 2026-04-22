// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

// Comparison test: validates compressed data against uncompressed reference.
// Reads the same timestep from both formats and compares field values.
// Verifies that SZ3 decompression produces values within tolerance.
//
// NOTE: Uncompressed files are 78-117 GB each. Requires many MPI ranks.

#include "vtkNek5000Reader.h"

#include "vtkDataArray.h"
#include "vtkMPIController.h"
#include "vtkNew.h"
#include "vtkPointData.h"
#include "vtkUnstructuredGrid.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>

int TestNek5000CompressedVsUncompressed(int argc, char* argv[])
{
  vtkNew<vtkMPIController> controller;
  controller->Initialize(&argc, &argv, 0);
  vtkMultiProcessController::SetGlobalController(controller);

  int myRank = controller->GetLocalProcessId();
  int numRanks = controller->GetNumberOfProcesses();

  // Check for required files
  const char* compressedFile = "/archives/disk1/viralss2/jicf_comp/cmpjicf0.f00001";
  const char* uncompressedFile = "/archives/disk1/viralss2/jicf_comp/uncjicf0.f00001";

  std::ifstream compFile(compressedFile);
  std::ifstream uncFile(uncompressedFile);

  if (!compFile.good() || !uncFile.good())
  {
    if (myRank == 0)
    {
      std::cout << "Skipping test - data files not found" << std::endl;
      if (!compFile.good())
        std::cout << "  Missing: " << compressedFile << std::endl;
      if (!uncFile.good())
        std::cout << "  Missing: " << uncompressedFile << std::endl;
    }
    controller->Finalize();
    return EXIT_SUCCESS;
  }
  compFile.close();
  uncFile.close();

  // Warn about memory requirements
  if (myRank == 0)
  {
    std::cout << "=== Compressed vs Uncompressed Comparison Test ===" << std::endl;
    std::cout << "MPI Ranks: " << numRanks << std::endl;
    std::cout << "WARNING: Uncompressed files are 78-117 GB each." << std::endl;
    std::cout << "         Recommend running with >= 32 MPI ranks." << std::endl;
  }

  // Create index files for both formats
  const char* compIndexPath = "/tmp/test_compressed.nek5000";
  const char* uncIndexPath = "/tmp/test_uncompressed.nek5000";

  if (myRank == 0)
  {
    // Compressed index (timestep 1 maps to f00001)
    std::ofstream compIdx(compIndexPath);
    compIdx << " filetemplate: /archives/disk1/viralss2/jicf_comp/cmpjicf%01d.f%05d\n";
    compIdx << " firsttimestep: 1\n";
    compIdx << " numtimesteps: 1\n";
    compIdx.close();

    // Uncompressed index (timestep 1 maps to f00001)
    std::ofstream uncIdx(uncIndexPath);
    uncIdx << " filetemplate: /archives/disk1/viralss2/jicf_comp/uncjicf%01d.f%05d\n";
    uncIdx << " firsttimestep: 1\n";
    uncIdx << " numtimesteps: 1\n";
    uncIdx.close();
  }
  controller->Barrier();

  // Read compressed data
  if (myRank == 0)
  {
    std::cout << "\nReading compressed data..." << std::endl;
  }

  vtkNew<vtkNek5000Reader> compReader;
  compReader->SetFileName(compIndexPath);
  compReader->EnableAllPointArrays();
  compReader->Update();

  vtkUnstructuredGrid* compOutput = compReader->GetOutput();
  if (!compOutput || compOutput->GetNumberOfPoints() == 0)
  {
    std::cerr << "Rank " << myRank << ": Failed to read compressed data" << std::endl;
    controller->Finalize();
    return EXIT_FAILURE;
  }

  if (myRank == 0)
  {
    std::cout << "  Compressed data loaded" << std::endl;
    std::cout << "\nReading uncompressed data..." << std::endl;
  }

  // Read uncompressed data
  vtkNew<vtkNek5000Reader> uncReader;
  uncReader->SetFileName(uncIndexPath);
  uncReader->EnableAllPointArrays();
  uncReader->Update();

  vtkUnstructuredGrid* uncOutput = uncReader->GetOutput();
  if (!uncOutput || uncOutput->GetNumberOfPoints() == 0)
  {
    std::cerr << "Rank " << myRank << ": Failed to read uncompressed data" << std::endl;
    controller->Finalize();
    return EXIT_FAILURE;
  }

  if (myRank == 0)
  {
    std::cout << "  Uncompressed data loaded" << std::endl;
  }

  // Verify same point count locally
  vtkIdType compPoints = compOutput->GetNumberOfPoints();
  vtkIdType uncPoints = uncOutput->GetNumberOfPoints();

  if (compPoints != uncPoints)
  {
    std::cerr << "Rank " << myRank << ": Point count mismatch - compressed=" << compPoints
              << " uncompressed=" << uncPoints << std::endl;
    controller->Finalize();
    return EXIT_FAILURE;
  }

  // Compare field arrays
  vtkPointData* compPD = compOutput->GetPointData();
  vtkPointData* uncPD = uncOutput->GetPointData();

  const char* fieldNames[] = {"Velocity", "Pressure", "Temperature"};
  const int numFields = 3;

  double localMaxDiff[3] = {0, 0, 0};
  double localMaxRelDiff[3] = {0, 0, 0};

  for (int f = 0; f < numFields; f++)
  {
    vtkDataArray* compArr = compPD->GetArray(fieldNames[f]);
    vtkDataArray* uncArr = uncPD->GetArray(fieldNames[f]);

    if (!compArr || !uncArr)
    {
      if (myRank == 0)
      {
        std::cerr << "WARNING: Missing array " << fieldNames[f] << std::endl;
      }
      continue;
    }

    int numComps = compArr->GetNumberOfComponents();
    vtkIdType numTuples = compArr->GetNumberOfTuples();

    for (vtkIdType i = 0; i < numTuples; i++)
    {
      for (int c = 0; c < numComps; c++)
      {
        double compVal = compArr->GetComponent(i, c);
        double uncVal = uncArr->GetComponent(i, c);

        double absDiff = std::abs(compVal - uncVal);
        double relDiff = (std::abs(uncVal) > 1e-10) ? absDiff / std::abs(uncVal) : absDiff;

        if (absDiff > localMaxDiff[f])
          localMaxDiff[f] = absDiff;
        if (relDiff > localMaxRelDiff[f])
          localMaxRelDiff[f] = relDiff;
      }
    }
  }

  // Reduce to get global max differences
  double globalMaxDiff[3] = {0, 0, 0};
  double globalMaxRelDiff[3] = {0, 0, 0};

  controller->Reduce(localMaxDiff, globalMaxDiff, 3, vtkCommunicator::MAX_OP, 0);
  controller->Reduce(localMaxRelDiff, globalMaxRelDiff, 3, vtkCommunicator::MAX_OP, 0);

  bool success = true;

  if (myRank == 0)
  {
    std::cout << "\n=== Comparison Results ===" << std::endl;

    // SZ3 tolerance (typical error bound used in compression)
    const double absTolerance = 1e-2;  // Absolute error bound
    const double relTolerance = 1e-2;  // 1% relative error

    for (int f = 0; f < numFields; f++)
    {
      std::cout << fieldNames[f] << ":" << std::endl;
      std::cout << "  Max absolute diff: " << globalMaxDiff[f] << std::endl;
      std::cout << "  Max relative diff: " << globalMaxRelDiff[f] * 100 << "%" << std::endl;

      if (globalMaxDiff[f] > absTolerance && globalMaxRelDiff[f] > relTolerance)
      {
        std::cerr << "  ERROR: Difference exceeds tolerance!" << std::endl;
        success = false;
      }
      else
      {
        std::cout << "  OK - within SZ3 tolerance" << std::endl;
      }
    }

    if (success)
    {
      std::cout << "\n=== COMPARISON TEST PASSED ===" << std::endl;
    }
    else
    {
      std::cout << "\n=== COMPARISON TEST FAILED ===" << std::endl;
    }
  }

  controller->Finalize();
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
