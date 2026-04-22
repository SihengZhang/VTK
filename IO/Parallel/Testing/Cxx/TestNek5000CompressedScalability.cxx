// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

// MPI Scalability test for compressed Nek5000 reader.
// Tests correct data distribution with varying MPI rank counts (8, 16, 32).
// Verifies total points/cells are consistent regardless of rank count.

#include "vtkNek5000Reader.h"

#include "vtkDataArray.h"
#include "vtkMPIController.h"
#include "vtkNew.h"
#include "vtkPointData.h"
#include "vtkTimerLog.h"
#include "vtkUnstructuredGrid.h"

#include <cstdlib>
#include <fstream>
#include <iostream>

int TestNek5000CompressedScalability(int argc, char* argv[])
{
  vtkNew<vtkMPIController> controller;
  controller->Initialize(&argc, &argv, 0);
  vtkMultiProcessController::SetGlobalController(controller);

  int myRank = controller->GetLocalProcessId();
  int numRanks = controller->GetNumberOfProcesses();

  // Path to compressed data
  const char* dataFile = "/archives/disk1/viralss2/jicf_comp/cmpjicf0.f00001";

  std::ifstream testFile(dataFile);
  if (!testFile.good())
  {
    if (myRank == 0)
    {
      std::cout << "Skipping test - data file not found: " << dataFile << std::endl;
    }
    controller->Finalize();
    return EXIT_SUCCESS;
  }
  testFile.close();

  // Create nek5000 index file
  const char* indexPath = "/tmp/test_scalability.nek5000";
  if (myRank == 0)
  {
    std::ofstream nek5000File(indexPath);
    nek5000File << " filetemplate: /archives/disk1/viralss2/jicf_comp/cmpjicf%01d.f%05d\n";
    nek5000File << " firsttimestep: 1\n";
    nek5000File << " numtimesteps: 1\n";
    nek5000File.close();
  }
  controller->Barrier();

  if (myRank == 0)
  {
    std::cout << "=== Compressed Reader Scalability Test ===" << std::endl;
    std::cout << "MPI Ranks: " << numRanks << std::endl;
  }

  // Start timing
  vtkNew<vtkTimerLog> timer;
  timer->StartTimer();

  vtkNew<vtkNek5000Reader> reader;
  reader->SetFileName(indexPath);
  reader->EnableAllPointArrays();
  reader->Update();

  timer->StopTimer();
  double readTime = timer->GetElapsedTime();

  vtkUnstructuredGrid* output = reader->GetOutput();

  if (!output)
  {
    std::cerr << "Rank " << myRank << ": ERROR - null output" << std::endl;
    controller->Finalize();
    return EXIT_FAILURE;
  }

  // Gather statistics
  vtkIdType localPoints = output->GetNumberOfPoints();
  vtkIdType localCells = output->GetNumberOfCells();

  vtkIdType totalPoints = 0;
  vtkIdType totalCells = 0;
  vtkIdType minPoints = 0;
  vtkIdType maxPoints = 0;
  double maxTime = 0;

  controller->Reduce(&localPoints, &totalPoints, 1, vtkCommunicator::SUM_OP, 0);
  controller->Reduce(&localCells, &totalCells, 1, vtkCommunicator::SUM_OP, 0);
  controller->Reduce(&localPoints, &minPoints, 1, vtkCommunicator::MIN_OP, 0);
  controller->Reduce(&localPoints, &maxPoints, 1, vtkCommunicator::MAX_OP, 0);
  controller->Reduce(&readTime, &maxTime, 1, vtkCommunicator::MAX_OP, 0);

  // Get field ranges
  double velRange[2] = {0, 0};
  double presRange[2] = {0, 0};
  double tempRange[2] = {0, 0};

  vtkPointData* pd = output->GetPointData();
  if (pd)
  {
    vtkDataArray* vel = pd->GetArray("Velocity");
    if (vel) vel->GetRange(velRange);

    vtkDataArray* pres = pd->GetArray("Pressure");
    if (pres) pres->GetRange(presRange);

    vtkDataArray* temp = pd->GetArray("Temperature");
    if (temp) temp->GetRange(tempRange);
  }

  // Reduce to get global ranges
  double globalVelMin = 0, globalVelMax = 0;
  double globalPresMin = 0, globalPresMax = 0;
  double globalTempMin = 0, globalTempMax = 0;

  controller->Reduce(&velRange[0], &globalVelMin, 1, vtkCommunicator::MIN_OP, 0);
  controller->Reduce(&velRange[1], &globalVelMax, 1, vtkCommunicator::MAX_OP, 0);
  controller->Reduce(&presRange[0], &globalPresMin, 1, vtkCommunicator::MIN_OP, 0);
  controller->Reduce(&presRange[1], &globalPresMax, 1, vtkCommunicator::MAX_OP, 0);
  controller->Reduce(&tempRange[0], &globalTempMin, 1, vtkCommunicator::MIN_OP, 0);
  controller->Reduce(&tempRange[1], &globalTempMax, 1, vtkCommunicator::MAX_OP, 0);

  bool success = true;

  if (myRank == 0)
  {
    std::cout << "\nResults:" << std::endl;
    std::cout << "  Total points: " << totalPoints << std::endl;
    std::cout << "  Total cells:  " << totalCells << std::endl;
    std::cout << "  Points per rank: min=" << minPoints << " max=" << maxPoints << std::endl;
    std::cout << "  Read time (max): " << maxTime << " seconds" << std::endl;
    std::cout << "\nField ranges:" << std::endl;
    std::cout << "  Velocity: [" << globalVelMin << ", " << globalVelMax << "]" << std::endl;
    std::cout << "  Pressure: [" << globalPresMin << ", " << globalPresMax << "]" << std::endl;
    std::cout << "  Temperature: [" << globalTempMin << ", " << globalTempMax << "]" << std::endl;

    // Expected values from previous 8-rank test
    const vtkIdType expectedPoints = 3240264000LL;
    const vtkIdType expectedCells = 2362152456LL;

    // Verify totals
    if (totalPoints != expectedPoints)
    {
      std::cerr << "ERROR: Expected " << expectedPoints << " points, got " << totalPoints << std::endl;
      success = false;
    }
    if (totalCells != expectedCells)
    {
      std::cerr << "ERROR: Expected " << expectedCells << " cells, got " << totalCells << std::endl;
      success = false;
    }

    // Verify no empty ranks (unless more ranks than writer ranks)
    if (minPoints == 0 && numRanks <= 1620)
    {
      std::cerr << "ERROR: Some ranks have no data" << std::endl;
      success = false;
    }

    // Verify reasonable field ranges
    if (globalVelMax < 0.1 || globalVelMax > 10.0)
    {
      std::cerr << "WARNING: Unusual velocity range" << std::endl;
    }

    if (success)
    {
      std::cout << "\n=== SCALABILITY TEST PASSED ===" << std::endl;
    }
    else
    {
      std::cout << "\n=== SCALABILITY TEST FAILED ===" << std::endl;
    }
  }

  controller->Finalize();
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
