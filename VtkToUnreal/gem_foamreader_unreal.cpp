// FoamDataReader.cpp
#include "FoamDataReader.h"
#include "Paths.h"

// VTK Includes (ensure these are correctly linked in your Build.cs)
#include "vtkOpenFOAMReader.h"
#include "vtkDataSetSurfaceFilter.h"
#include "vtkNew.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkPolyData.h"

UFoamDataReaderComponent::UFoamDataReaderComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

vtkPolyData* UFoamDataReaderComponent::ReadFoamFile(const FString& FoamFilePath)
{
    // Check if the provided path is valid
    if (FoamFilePath.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Foam file path is empty."));
        return nullptr;
    }

    FString FullPath = FPaths::ProjectContentDir() / FoamFilePath;
    if (!FPaths::DirectoryExists(FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Foam directory not found at: %s"), *FullPath);
        return nullptr;
    }

    // Create a new VTK OpenFOAM reader
    vtkNew<vtkOpenFOAMReader> foamReader;

    // Set the file name (VTK expects UTF8 encoding for file paths)
    foamReader->SetFileName(TCHAR_TO_UTF8(*FullPath));
    UE_LOG(LogTemp, Log, TEXT("Reading foam file from: %s"), *FullPath);

    // This is crucial for OpenFOAM data. We need to set the time step.
    // The easiest way is to read the latest time step.
    foamReader->SetTimeStep(foamReader->GetNumberOfTimeSteps() - 1);

    // Update the reader to execute the pipeline
    foamReader->Update();

    // The output of vtkOpenFOAMReader is a vtkMultiBlockDataSet, which
    // is a hierarchical collection of datasets. We need to extract the first block,
    // which usually contains the main geometry.
    vtkMultiBlockDataSet* multiBlockData = vtkMultiBlockDataSet::SafeDownCast(foamReader->GetOutput());
    if (!multiBlockData || multiBlockData->GetNumberOfBlocks() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to read foam data or it's empty."));
        return nullptr;
    }

    // Get the first block, which is typically the geometry data
    vtkDataSet* dataSet = vtkDataSet::SafeDownCast(multiBlockData->GetBlock(0));
    if (!dataSet)
    {
        UE_LOG(LogTemp, Error, TEXT("The first block of the foam data is not a vtkDataSet."));
        return nullptr;
    }

    // Use vtkDataSetSurfaceFilter to extract the surface from the vtkDataSet
    // This will convert the unstructured grid data into polydata.
    vtkNew<vtkDataSetSurfaceFilter> surfaceFilter;
    surfaceFilter->SetInputData(dataSet);
    surfaceFilter->Update();

    // Get the output as a vtkPolyData object
    vtkPolyData* polyData = surfaceFilter->GetOutput();
    if (!polyData)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to extract surface polydata from the foam data."));
        return nullptr;
    }

    // Return the vtkPolyData object
    // NOTE: This object is now owned by the caller. You must call `polyData->Delete()`
    // after you have finished using it to prevent memory leaks.
    return polyData;
}
