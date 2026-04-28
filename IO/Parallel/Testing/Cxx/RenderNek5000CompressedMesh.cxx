// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

// Render complete mesh surface of compressed Nek5000 data at 4K resolution.
// Uses parallel compositing - each rank renders its portion and composites.

#include "vtkNek5000Reader.h"

#include "vtkActor.h"
#include "vtkCamera.h"
#include "vtkCompositeRenderManager.h"
#include "vtkDataSetMapper.h"
#include "vtkMPIController.h"
#include "vtkNew.h"
#include "vtkPNGWriter.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkUnstructuredGrid.h"
#include "vtkWindowToImageFilter.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

int RenderNek5000CompressedMesh(int argc, char* argv[])
{
  vtkNew<vtkMPIController> controller;
  controller->Initialize(&argc, &argv, 0);
  vtkMultiProcessController::SetGlobalController(controller);

  int myRank = controller->GetLocalProcessId();
  int numRanks = controller->GetNumberOfProcesses();

  // Check for data file
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

  // Create output directory
  std::string outputDir = std::string(getenv("HOME")) + "/VTK";
  if (myRank == 0)
  {
    mkdir(outputDir.c_str(), 0755);
    std::cout << "=== Rendering Compressed Mesh Surface (4K) ===" << std::endl;
    std::cout << "MPI Ranks: " << numRanks << std::endl;
    std::cout << "Output directory: " << outputDir << std::endl;
  }

  // Create index file for first timestep only
  const char* indexPath = "/tmp/render_mesh_surface.nek5000";
  if (myRank == 0)
  {
    std::ofstream nek5000File(indexPath);
    nek5000File << " filetemplate: /archives/disk1/viralss2/jicf_comp/cmpjicf%01d.f%05d\n";
    nek5000File << " firsttimestep: 1\n";
    nek5000File << " numtimesteps: 1\n";
    nek5000File.close();
  }
  controller->Barrier();

  // Read data - only need geometry, no field arrays
  if (myRank == 0)
  {
    std::cout << "Reading compressed mesh data..." << std::endl;
  }

  vtkNew<vtkNek5000Reader> reader;
  reader->SetFileName(indexPath);
  reader->DisableAllPointArrays();  // Only need geometry
  reader->Update();

  vtkUnstructuredGrid* data = reader->GetOutput();
  if (!data || data->GetNumberOfPoints() == 0)
  {
    std::cerr << "Rank " << myRank << ": Failed to read data" << std::endl;
    controller->Finalize();
    return EXIT_FAILURE;
  }

  vtkIdType localPoints = data->GetNumberOfPoints();
  vtkIdType localCells = data->GetNumberOfCells();

  // Get local bounds
  double localBounds[6];
  data->GetBounds(localBounds);

  // Compute global bounds across all ranks
  double globalBounds[6];
  controller->Reduce(&localBounds[0], &globalBounds[0], 1, vtkCommunicator::MIN_OP, 0);
  controller->Reduce(&localBounds[1], &globalBounds[1], 1, vtkCommunicator::MAX_OP, 0);
  controller->Reduce(&localBounds[2], &globalBounds[2], 1, vtkCommunicator::MIN_OP, 0);
  controller->Reduce(&localBounds[3], &globalBounds[3], 1, vtkCommunicator::MAX_OP, 0);
  controller->Reduce(&localBounds[4], &globalBounds[4], 1, vtkCommunicator::MIN_OP, 0);
  controller->Reduce(&localBounds[5], &globalBounds[5], 1, vtkCommunicator::MAX_OP, 0);
  controller->Broadcast(globalBounds, 6, 0);

  // Count total points and cells
  vtkIdType totalPoints = 0, totalCells = 0;
  controller->Reduce(&localPoints, &totalPoints, 1, vtkCommunicator::SUM_OP, 0);
  controller->Reduce(&localCells, &totalCells, 1, vtkCommunicator::SUM_OP, 0);

  if (myRank == 0)
  {
    std::cout << "Global bounds: X[" << globalBounds[0] << ", " << globalBounds[1] << "] "
              << "Y[" << globalBounds[2] << ", " << globalBounds[3] << "] "
              << "Z[" << globalBounds[4] << ", " << globalBounds[5] << "]" << std::endl;
    std::cout << "Total mesh: " << totalPoints << " points, " << totalCells << " cells" << std::endl;
    std::cout << "Rank " << myRank << ": " << localPoints << " points, "
              << localCells << " cells" << std::endl;
  }

  // Setup mapper - render directly from unstructured grid
  if (myRank == 0)
  {
    std::cout << "Setting up parallel renderer..." << std::endl;
  }

  vtkNew<vtkDataSetMapper> mapper;
  mapper->SetInputData(data);
  mapper->ScalarVisibilityOff();

  // Setup actor with surface properties
  vtkNew<vtkActor> actor;
  actor->SetMapper(mapper);
  actor->GetProperty()->SetRepresentationToSurface();
  actor->GetProperty()->SetColor(0.8, 0.8, 0.9);  // Light blue-gray
  actor->GetProperty()->SetAmbient(0.3);
  actor->GetProperty()->SetDiffuse(0.6);
  actor->GetProperty()->SetSpecular(0.2);
  actor->GetProperty()->SetSpecularPower(20);

  // Setup renderer
  vtkNew<vtkRenderer> renderer;
  renderer->AddActor(actor);
  renderer->SetBackground(0.1, 0.1, 0.15);  // Dark background
  renderer->SetBackground2(0.2, 0.2, 0.25);
  renderer->GradientBackgroundOn();

  // Setup camera for good view of the mesh
  double centerX = (globalBounds[0] + globalBounds[1]) / 2.0;
  double centerY = (globalBounds[2] + globalBounds[3]) / 2.0;
  double centerZ = (globalBounds[4] + globalBounds[5]) / 2.0;

  double rangeX = globalBounds[1] - globalBounds[0];
  double rangeY = globalBounds[3] - globalBounds[2];
  double rangeZ = globalBounds[5] - globalBounds[4];
  double maxRange = std::max({rangeX, rangeY, rangeZ});

  vtkCamera* camera = renderer->GetActiveCamera();
  // Position camera at an angle for 3D view
  camera->SetPosition(centerX + maxRange * 1.5,
                      centerY + maxRange * 1.2,
                      centerZ + maxRange * 1.0);
  camera->SetFocalPoint(centerX, centerY, centerZ);
  camera->SetViewUp(0, 0, 1);

  renderer->ResetCameraClippingRange();

  // Setup render window at 4K resolution
  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->AddRenderer(renderer);
  renderWindow->SetSize(3840, 2160);  // 4K resolution
  renderWindow->OffScreenRenderingOn();

  // Setup parallel compositing
  vtkNew<vtkCompositeRenderManager> renderManager;
  renderManager->SetRenderWindow(renderWindow);
  renderManager->SetController(controller);
  renderManager->InitializePieces();
  renderManager->InitializeOffScreen();

  if (myRank == 0)
  {
    std::cout << "Rendering at 4K resolution (3840x2160)..." << std::endl;
  }

  // Render with compositing
  renderManager->ResetAllCameras();
  renderWindow->Render();
  renderManager->StartServices();

  controller->Barrier();

  // Only rank 0 saves the image
  if (myRank == 0)
  {
    // Capture image
    vtkNew<vtkWindowToImageFilter> windowToImage;
    windowToImage->SetInput(renderWindow);
    windowToImage->SetScale(1);
    windowToImage->SetInputBufferTypeToRGBA();
    windowToImage->Update();

    // Save to PNG
    std::string filename = outputDir + "/compressed_mesh_4k.png";

    vtkNew<vtkPNGWriter> writer;
    writer->SetFileName(filename.c_str());
    writer->SetInputConnection(windowToImage->GetOutputPort());
    writer->Write();

    std::cout << "\n=== RENDERING COMPLETE ===" << std::endl;
    std::cout << "Image saved to: " << filename << std::endl;
    std::cout << "Resolution: 3840x2160 (4K)" << std::endl;
  }

  renderManager->StopServices();
  controller->Finalize();
  return EXIT_SUCCESS;
}
