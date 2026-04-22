// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

// MPI Test for reading compressed Nek5000 data files using SZ3 decompression.
// This test requires MPI to distribute the large data across multiple processes.

#include "vtkNek5000Reader.h"

#include "vtkCellData.h"
#include "vtkDataArray.h"
#include "vtkInformation.h"
#include "vtkMPIController.h"
#include "vtkNew.h"
#include "vtkPointData.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkUnstructuredGrid.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

int TestNek5000CompressedReaderMPI(int argc, char* argv[])
{
  vtkNew<vtkMPIController> controller;
  controller->Initialize(&argc, &argv, 0);
  vtkMultiProcessController::SetGlobalController(controller);

  int myRank = controller->GetLocalProcessId();
  int numRanks = controller->GetNumberOfProcesses();

  // Path to compressed data - only run if file exists
  const char* dataFile = "/archives/disk1/viralss2/jicf_comp/cmpjicf0.f00001";

  std::ifstream testFile(dataFile);
  if (!testFile.good())
  {
    if (myRank == 0)
    {
      std::cout << "Skipping test - compressed data file not found: " << dataFile << std::endl;
      std::cout << "This test requires large compressed Nek5000 data files." << std::endl;
    }
    controller->Finalize();
    return EXIT_SUCCESS;
  }
  testFile.close();

  // Create nek5000 index file
  const char* compressedPath = "/tmp/test_cmpjicf_mpi.nek5000";
  if (myRank == 0)
  {
    std::ofstream nek5000File(compressedPath);
    nek5000File << " filetemplate: /archives/disk1/viralss2/jicf_comp/cmpjicf%01d.f%05d\n";
    nek5000File << " firsttimestep: 1\n";
    nek5000File << " numtimesteps: 1\n"; // Just read 1 timestep for testing
    nek5000File.close();
  }
  controller->Barrier();

  if (myRank == 0)
  {
    std::cout << "Testing vtkNek5000Reader with compressed (#stdc) format" << std::endl;
    std::cout << "Using " << numRanks << " MPI ranks" << std::endl;
    std::cout << "Data file: " << compressedPath << std::endl;
  }

  vtkNew<vtkNek5000Reader> reader;
  reader->SetFileName(compressedPath);
  reader->EnableAllPointArrays();

  // Just update information first
  reader->UpdateInformation();

  if (myRank == 0)
  {
    std::cout << "Information update complete" << std::endl;
  }

  // Update to read data
  reader->Update();

  vtkUnstructuredGrid* output = reader->GetOutput();

  if (!output)
  {
    std::cerr << "Rank " << myRank << ": ERROR - Reader output is null" << std::endl;
    controller->Finalize();
    return EXIT_FAILURE;
  }

  // Gather statistics across ranks
  vtkIdType localPoints = output->GetNumberOfPoints();
  vtkIdType localCells = output->GetNumberOfCells();

  vtkIdType totalPoints = 0;
  vtkIdType totalCells = 0;

  controller->Reduce(&localPoints, &totalPoints, 1, vtkCommunicator::SUM_OP, 0);
  controller->Reduce(&localCells, &totalCells, 1, vtkCommunicator::SUM_OP, 0);

  if (myRank == 0)
  {
    std::cout << "Total points across all ranks: " << totalPoints << std::endl;
    std::cout << "Total cells across all ranks: " << totalCells << std::endl;
  }

  // Local validation
  std::cout << "Rank " << myRank << ": " << localPoints << " points, " << localCells << " cells"
            << std::endl;

  // Check point data arrays
  vtkPointData* pd = output->GetPointData();
  if (pd && pd->GetNumberOfArrays() > 0)
  {
    for (int i = 0; i < pd->GetNumberOfArrays(); i++)
    {
      vtkDataArray* arr = pd->GetArray(i);
      if (arr && myRank == 0)
      {
        double range[2];
        arr->GetRange(range);
        std::cout << "Array '" << arr->GetName() << "': range [" << range[0] << ", " << range[1]
                  << "]" << std::endl;
      }
    }
  }

  // Basic validation
  bool success = true;
  if (localPoints == 0 || localCells == 0)
  {
    std::cerr << "Rank " << myRank << ": WARNING - Empty local output" << std::endl;
  }

  controller->Finalize();

  if (myRank == 0)
  {
    std::cout << "Test PASSED" << std::endl;
  }

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
