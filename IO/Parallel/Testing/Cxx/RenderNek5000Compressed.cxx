// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

// Render 10 z-x slices of compressed Nek5000 data showing pressure field.
// Slices are evenly distributed along Y axis. All MPI ranks contribute.

#include "vtkNek5000Reader.h"

#include "vtkActor.h"
#include "vtkAppendPolyData.h"
#include "vtkCamera.h"
#include "vtkColorTransferFunction.h"
#include "vtkCutter.h"
#include "vtkDataSetMapper.h"
#include "vtkMPIController.h"
#include "vtkNew.h"
#include "vtkPNGWriter.h"
#include "vtkPlane.h"
#include "vtkPointData.h"
#include "vtkPolyDataMapper.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkUnstructuredGrid.h"
#include "vtkWindowToImageFilter.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

// Create a diverging red-blue color map centered at midpoint
static vtkSmartPointer<vtkColorTransferFunction> CreateRedBlueColorMap(double minVal, double maxVal)
{
  vtkNew<vtkColorTransferFunction> ctf;
  ctf->SetColorSpaceToDiverging();

  // For pressure, center at zero if range spans zero, otherwise at midpoint
  double midVal = (minVal + maxVal) / 2.0;
  if (minVal < 0 && maxVal > 0)
  {
    midVal = 0.0;
  }

  // Blue (low) -> White (mid) -> Red (high)
  ctf->AddRGBPoint(minVal, 0.230, 0.299, 0.754);  // Blue
  ctf->AddRGBPoint(midVal, 0.865, 0.865, 0.865);  // White
  ctf->AddRGBPoint(maxVal, 0.706, 0.016, 0.150);  // Red

  return ctf;
}

int RenderNek5000Compressed(int argc, char* argv[])
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
  std::string outputDir = std::string(getenv("HOME")) + "/VTK/pressure_slices";
  if (myRank == 0)
  {
    mkdir(outputDir.c_str(), 0755);
    std::cout << "=== Rendering 10 Z-X Pressure Slices ===" << std::endl;
    std::cout << "MPI Ranks: " << numRanks << std::endl;
    std::cout << "Output directory: " << outputDir << std::endl;
  }

  // Create index file for first timestep only
  const char* indexPath = "/tmp/render_pressure_slices.nek5000";
  if (myRank == 0)
  {
    std::ofstream nek5000File(indexPath);
    nek5000File << " filetemplate: /archives/disk1/viralss2/jicf_comp/cmpjicf%01d.f%05d\n";
    nek5000File << " firsttimestep: 1\n";
    nek5000File << " numtimesteps: 1\n";
    nek5000File.close();
  }
  controller->Barrier();

  // Read data
  if (myRank == 0)
  {
    std::cout << "Reading compressed data..." << std::endl;
  }

  vtkNew<vtkNek5000Reader> reader;
  reader->SetFileName(indexPath);
  reader->EnableAllPointArrays();
  reader->Update();

  vtkUnstructuredGrid* data = reader->GetOutput();
  if (!data || data->GetNumberOfPoints() == 0)
  {
    std::cerr << "Rank " << myRank << ": Failed to read data" << std::endl;
    controller->Finalize();
    return EXIT_FAILURE;
  }

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

  if (myRank == 0)
  {
    std::cout << "Global bounds: X[" << globalBounds[0] << ", " << globalBounds[1] << "] "
              << "Y[" << globalBounds[2] << ", " << globalBounds[3] << "] "
              << "Z[" << globalBounds[4] << ", " << globalBounds[5] << "]" << std::endl;
  }

  // First pass: compute global pressure statistics across all slices
  const int numSlices = 10;
  double yMin = globalBounds[2];
  double yMax = globalBounds[3];
  double yStep = (yMax - yMin) / (numSlices + 1);

  double globalPressureMin = 1e30;
  double globalPressureMax = -1e30;
  double globalSum = 0.0;
  double globalSumSq = 0.0;
  vtkIdType globalCount = 0;

  for (int sliceIdx = 0; sliceIdx < numSlices; sliceIdx++)
  {
    double sliceY = yMin + (sliceIdx + 1) * yStep;

    vtkNew<vtkPlane> plane;
    plane->SetOrigin(0, sliceY, 0);
    plane->SetNormal(0, 1, 0);

    vtkNew<vtkCutter> cutter;
    cutter->SetInputData(data);
    cutter->SetCutFunction(plane);
    cutter->Update();

    vtkPolyData* localSlice = cutter->GetOutput();
    vtkDataArray* pressureArray = localSlice->GetPointData()->GetArray("Pressure");

    if (pressureArray && localSlice->GetNumberOfPoints() > 0)
    {
      double localRange[2];
      pressureArray->GetRange(localRange);
      if (localRange[0] < globalPressureMin) globalPressureMin = localRange[0];
      if (localRange[1] > globalPressureMax) globalPressureMax = localRange[1];

      for (vtkIdType i = 0; i < pressureArray->GetNumberOfTuples(); i++)
      {
        double val = pressureArray->GetTuple1(i);
        globalSum += val;
        globalSumSq += val * val;
        globalCount++;
      }
    }
  }

  // Reduce to get global statistics
  double allMin, allMax, allSum, allSumSq;
  vtkIdType allCount;
  controller->Reduce(&globalPressureMin, &allMin, 1, vtkCommunicator::MIN_OP, 0);
  controller->Reduce(&globalPressureMax, &allMax, 1, vtkCommunicator::MAX_OP, 0);
  controller->Reduce(&globalSum, &allSum, 1, vtkCommunicator::SUM_OP, 0);
  controller->Reduce(&globalSumSq, &allSumSq, 1, vtkCommunicator::SUM_OP, 0);
  controller->Reduce(&globalCount, &allCount, 1, vtkCommunicator::SUM_OP, 0);

  double mean = 0.0, stddev = 0.0;
  if (allCount > 0)
  {
    mean = allSum / allCount;
    stddev = std::sqrt(allSumSq / allCount - mean * mean);
  }

  // Use mean +/- 3*stddev for color range
  double colorRange[2];
  colorRange[0] = mean - 3.0 * stddev;
  colorRange[1] = mean + 3.0 * stddev;
  if (colorRange[0] < allMin) colorRange[0] = allMin;
  if (colorRange[1] > allMax) colorRange[1] = allMax;

  controller->Broadcast(&allMin, 1, 0);
  controller->Broadcast(&allMax, 1, 0);
  controller->Broadcast(colorRange, 2, 0);

  if (myRank == 0)
  {
    std::cout << "Pressure range: [" << allMin << ", " << allMax << "]" << std::endl;
    std::cout << "Pressure mean: " << mean << ", stddev: " << stddev << std::endl;
    std::cout << "Color range: [" << colorRange[0] << ", " << colorRange[1] << "]" << std::endl;
    std::cout << "\nRendering " << numSlices << " slices..." << std::endl;
  }

  // Second pass: render each slice
  for (int sliceIdx = 0; sliceIdx < numSlices; sliceIdx++)
  {
    double sliceY = yMin + (sliceIdx + 1) * yStep;

    if (myRank == 0)
    {
      std::cout << "  Slice " << (sliceIdx + 1) << "/" << numSlices
                << " at Y = " << sliceY << std::endl;
    }

    // Create cutting plane
    vtkNew<vtkPlane> plane;
    plane->SetOrigin(0, sliceY, 0);
    plane->SetNormal(0, 1, 0);

    vtkNew<vtkCutter> cutter;
    cutter->SetInputData(data);
    cutter->SetCutFunction(plane);
    cutter->Update();

    vtkPolyData* localSlice = cutter->GetOutput();
    vtkIdType localSlicePoints = localSlice->GetNumberOfPoints();

    // Gather all slice data to rank 0
    vtkNew<vtkAppendPolyData> appendFilter;

    if (myRank == 0)
    {
      if (localSlicePoints > 0)
      {
        appendFilter->AddInputData(localSlice);
      }

      for (int r = 1; r < numRanks; r++)
      {
        vtkSmartPointer<vtkPolyData> receivedSlice = vtkSmartPointer<vtkPolyData>::New();
        controller->Receive(receivedSlice, r, 2000 + sliceIdx);
        if (receivedSlice->GetNumberOfPoints() > 0)
        {
          appendFilter->AddInputData(receivedSlice);
        }
      }

      appendFilter->Update();
    }
    else
    {
      controller->Send(localSlice, 0, 2000 + sliceIdx);
    }

    controller->Barrier();

    // Only rank 0 renders and saves the image
    if (myRank == 0)
    {
      vtkPolyData* fullSlice = appendFilter->GetOutput();

      if (fullSlice->GetNumberOfPoints() == 0)
      {
        std::cout << "    Warning: Empty slice at Y = " << sliceY << std::endl;
        continue;
      }

      // Create red-blue diverging color map
      auto ctf = CreateRedBlueColorMap(colorRange[0], colorRange[1]);

      // Setup mapper
      vtkNew<vtkPolyDataMapper> mapper;
      mapper->SetInputData(fullSlice);
      mapper->SetScalarModeToUsePointFieldData();
      mapper->SelectColorArray("Pressure");
      mapper->SetScalarRange(colorRange);
      mapper->SetLookupTable(ctf);

      // Setup actor
      vtkNew<vtkActor> actor;
      actor->SetMapper(mapper);

      // Setup renderer
      vtkNew<vtkRenderer> renderer;
      renderer->AddActor(actor);
      renderer->SetBackground(0.1, 0.1, 0.15);

      // Setup camera to look at z-x plane
      double sliceBounds[6];
      fullSlice->GetBounds(sliceBounds);

      double centerX = (sliceBounds[0] + sliceBounds[1]) / 2.0;
      double centerZ = (sliceBounds[4] + sliceBounds[5]) / 2.0;
      double rangeX = sliceBounds[1] - sliceBounds[0];
      double rangeZ = sliceBounds[5] - sliceBounds[4];
      double maxRange = std::max(rangeX, rangeZ);

      vtkCamera* camera = renderer->GetActiveCamera();
      camera->SetPosition(centerX, sliceY + maxRange * 2.0, centerZ);
      camera->SetFocalPoint(centerX, sliceY, centerZ);
      camera->SetViewUp(0, 0, 1);
      camera->SetParallelProjection(true);
      camera->SetParallelScale(maxRange * 0.55);

      renderer->ResetCameraClippingRange();

      // Setup offscreen render window
      vtkNew<vtkRenderWindow> renderWindow;
      renderWindow->AddRenderer(renderer);
      renderWindow->SetSize(1920, 1080);
      renderWindow->OffScreenRenderingOn();
      renderWindow->Render();

      // Capture image
      vtkNew<vtkWindowToImageFilter> windowToImage;
      windowToImage->SetInput(renderWindow);
      windowToImage->SetScale(1);
      windowToImage->SetInputBufferTypeToRGBA();
      windowToImage->Update();

      // Save to PNG with slice index
      std::ostringstream filename;
      filename << outputDir << "/pressure_slice_"
               << std::setfill('0') << std::setw(2) << (sliceIdx + 1)
               << "_y" << std::fixed << std::setprecision(2) << sliceY << ".png";

      vtkNew<vtkPNGWriter> writer;
      writer->SetFileName(filename.str().c_str());
      writer->SetInputConnection(windowToImage->GetOutputPort());
      writer->Write();
    }
  }

  if (myRank == 0)
  {
    std::cout << "\n=== RENDERING COMPLETE ===" << std::endl;
    std::cout << "Images saved to: " << outputDir << std::endl;
  }

  controller->Finalize();
  return EXIT_SUCCESS;
}
