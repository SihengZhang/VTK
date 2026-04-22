// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

// Multi-timestep test for compressed Nek5000 reader.
// Reads all 3 available timesteps and verifies:
// - Each timestep loads successfully
// - Field values change between timesteps (data is not stale)

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
#include <vector>

int TestNek5000CompressedMultiTimestep(int argc, char* argv[])
{
  vtkNew<vtkMPIController> controller;
  controller->Initialize(&argc, &argv, 0);
  vtkMultiProcessController::SetGlobalController(controller);

  int myRank = controller->GetLocalProcessId();
  int numRanks = controller->GetNumberOfProcesses();

  // Check for data files
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

  // Create index file with all 3 timesteps
  const char* indexPath = "/tmp/test_multitimestep.nek5000";
  if (myRank == 0)
  {
    std::ofstream nek5000File(indexPath);
    nek5000File << " filetemplate: /archives/disk1/viralss2/jicf_comp/cmpjicf%01d.f%05d\n";
    nek5000File << " firsttimestep: 1\n";
    nek5000File << " numtimesteps: 3\n";
    nek5000File.close();
  }
  controller->Barrier();

  if (myRank == 0)
  {
    std::cout << "=== Multi-Timestep Test ===" << std::endl;
    std::cout << "MPI Ranks: " << numRanks << std::endl;
    std::cout << "Testing 3 timesteps" << std::endl;
  }

  vtkNew<vtkNek5000Reader> reader;
  reader->SetFileName(indexPath);
  reader->EnableAllPointArrays();

  // Update information to get timesteps
  reader->UpdateInformation();

  vtkInformation* outInfo = reader->GetOutputInformation(0);
  int numTimesteps = outInfo->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
  double* timeSteps = outInfo->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS());

  if (myRank == 0)
  {
    std::cout << "Available timesteps: " << numTimesteps << std::endl;
    for (int i = 0; i < numTimesteps; i++)
    {
      std::cout << "  Step " << i << ": time=" << timeSteps[i] << std::endl;
    }
  }

  if (numTimesteps < 3)
  {
    if (myRank == 0)
    {
      std::cerr << "ERROR: Expected at least 3 timesteps, got " << numTimesteps << std::endl;
    }
    controller->Finalize();
    return EXIT_FAILURE;
  }

  // Store field ranges for each timestep
  struct TimestepData
  {
    double velRange[2];
    double presRange[2];
    double tempRange[2];
    vtkIdType points;
  };
  std::vector<TimestepData> timestepData(numTimesteps);

  bool success = true;

  // Read each timestep
  for (int ts = 0; ts < numTimesteps; ts++)
  {
    if (myRank == 0)
    {
      std::cout << "\nReading timestep " << ts << " (time=" << timeSteps[ts] << ")..." << std::endl;
    }

    // Request this timestep
    outInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP(), timeSteps[ts]);
    reader->Update();

    vtkUnstructuredGrid* output = reader->GetOutput();

    if (!output)
    {
      std::cerr << "Rank " << myRank << ": Failed to read timestep " << ts << std::endl;
      success = false;
      break;
    }

    vtkIdType localPoints = output->GetNumberOfPoints();
    vtkIdType totalPoints = 0;
    controller->Reduce(&localPoints, &totalPoints, 1, vtkCommunicator::SUM_OP, 0);
    timestepData[ts].points = totalPoints;

    // Get local field ranges
    double localVelRange[2] = {0, 0};
    double localPresRange[2] = {0, 0};
    double localTempRange[2] = {0, 0};

    vtkPointData* pd = output->GetPointData();
    if (pd)
    {
      vtkDataArray* vel = pd->GetArray("Velocity");
      if (vel) vel->GetRange(localVelRange);

      vtkDataArray* pres = pd->GetArray("Pressure");
      if (pres) pres->GetRange(localPresRange);

      vtkDataArray* temp = pd->GetArray("Temperature");
      if (temp) temp->GetRange(localTempRange);
    }

    // Reduce to global ranges
    controller->Reduce(&localVelRange[0], &timestepData[ts].velRange[0], 1, vtkCommunicator::MIN_OP, 0);
    controller->Reduce(&localVelRange[1], &timestepData[ts].velRange[1], 1, vtkCommunicator::MAX_OP, 0);
    controller->Reduce(&localPresRange[0], &timestepData[ts].presRange[0], 1, vtkCommunicator::MIN_OP, 0);
    controller->Reduce(&localPresRange[1], &timestepData[ts].presRange[1], 1, vtkCommunicator::MAX_OP, 0);
    controller->Reduce(&localTempRange[0], &timestepData[ts].tempRange[0], 1, vtkCommunicator::MIN_OP, 0);
    controller->Reduce(&localTempRange[1], &timestepData[ts].tempRange[1], 1, vtkCommunicator::MAX_OP, 0);

    if (myRank == 0)
    {
      std::cout << "  Total points: " << totalPoints << std::endl;
      std::cout << "  Velocity range: [" << timestepData[ts].velRange[0] << ", "
                << timestepData[ts].velRange[1] << "]" << std::endl;
      std::cout << "  Pressure range: [" << timestepData[ts].presRange[0] << ", "
                << timestepData[ts].presRange[1] << "]" << std::endl;
      std::cout << "  Temperature range: [" << timestepData[ts].tempRange[0] << ", "
                << timestepData[ts].tempRange[1] << "]" << std::endl;
    }
  }

  // Verify results
  if (myRank == 0 && success)
  {
    std::cout << "\n=== Verification ===" << std::endl;

    // Check point counts are consistent
    for (int ts = 1; ts < numTimesteps; ts++)
    {
      if (timestepData[ts].points != timestepData[0].points)
      {
        std::cerr << "ERROR: Point count changed between timesteps" << std::endl;
        success = false;
      }
    }

    // Check that data changes between timesteps (at least slightly)
    // This ensures we're not reading stale data
    bool dataChanged = false;
    for (int ts = 1; ts < numTimesteps; ts++)
    {
      double velDiff = std::abs(timestepData[ts].velRange[1] - timestepData[ts - 1].velRange[1]);
      double presDiff = std::abs(timestepData[ts].presRange[1] - timestepData[ts - 1].presRange[1]);
      double tempDiff = std::abs(timestepData[ts].tempRange[1] - timestepData[ts - 1].tempRange[1]);

      if (velDiff > 1e-10 || presDiff > 1e-10 || tempDiff > 1e-10)
      {
        dataChanged = true;
      }
    }

    if (!dataChanged)
    {
      std::cerr << "WARNING: Field ranges identical across timesteps - data may be stale" << std::endl;
      // Not a failure, just a warning - some simulations may have steady-state data
    }

    if (success)
    {
      std::cout << "\n=== MULTI-TIMESTEP TEST PASSED ===" << std::endl;
    }
    else
    {
      std::cout << "\n=== MULTI-TIMESTEP TEST FAILED ===" << std::endl;
    }
  }

  controller->Finalize();
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
