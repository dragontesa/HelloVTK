/****
 * 
 * To load decomposed OpenFOAM cases efficiently in a way similar to ParaView, 
 * we need to instruct the vtkOpenFOAMReader to recognize the decomposed structure.
 *  Since you have numberOfSubdomains 4, the reader needs to be set to "Decomposed Case" mode.
In Unreal, loading 1.3GB of data on the Game Thread will cause the engine to freeze.
 To improve performance, we should:
    Use CaseType = 1: This tells VTK to look for processorN directories.
    Enable Multithreading: Use Unreal's AsyncTask or ParallelFor to handle the conversion logic,
     though VTK's reader itself is generally single-threaded per instance unless linked with MPI.
     However, we can use vtkAppendPolyData to merge the blocks more efficiently.
    Optimize the pipeline: Use vtkDataSetSurfaceFilter immediately to discard internal volume cells and only keep the 50,000+ surface points.

    I have updated the FoamDataReader.cpp to handle these decomposed cases.
Parallel Foam Data Reader
1월 8일 오후 10:58
Key points for handling your 1.3GB file:

    SetCaseType(1): This is the magic switch. It tells the reader to ignore the reconstructed constant folder and 
    instead look for processor0 through processor3 folders as defined in your decomposeParDict.

    Memory Management: 1.3GB of ASCII foam data can easily expand to 4-5GB in RAM when converted to VTK objects.
     The vtkDataSetSurfaceFilter inside the loop is critical—it extracts the "skin" of the volume data.
      Since you only want to visualize the surface in Unreal, this reduces the point count significantly before the "Append" step.

    Threading Note: While VTK's Update() is internally handling the logic, 
    I recommend calling ReadFoamFile from inside an AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [...] ) in your Actor class.
     This prevents the Unreal UI from hanging for the 10-20 seconds it might take to parse 1.3GB of text.

    Field Selection: If you only need custom_table_scalars, you should call reader->DisableAllPointArrays() and then 
    reader->SetPointArrayStatus("your_scalar_name", 1) before calling Update(). 
    This prevents the reader from loading velocity (U), pressure (p), and other heavy tensors into memory.

 */
*/


// FoamDataReader.cpp
#include "FoamDataReader.h"
#include "Paths.h"
#include "Async/Async.h"
#include "HAL/RunnableThread.h"

// VTK Includes
#include "vtkOpenFOAMReader.h"
#include "vtkDataSetSurfaceFilter.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkPolyData.h"
#include "vtkAppendPolyData.h"
#include "vtkNew.h"
#include "vtkInformation.h"

UFoamDataReaderComponent::UFoamDataReaderComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

vtkPolyData* UFoamDataReaderComponent::ReadFoamFile(const FString& FoamFilePath)
{
    if (FoamFilePath.IsEmpty()) return nullptr;

    FString FullPath = FPaths::ProjectContentDir() / FoamFilePath;
    if (!FPaths::FileExists(FullPath) && !FPaths::DirectoryExists(FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Foam path not found: %s"), *FullPath);
        return nullptr;
    }

    // Step 1: Initialize Reader for Decomposed Case
    vtkNew<vtkOpenFOAMReader> reader;
    reader->SetFileName(TCHAR_TO_UTF8(*FullPath));
    
    // 0 = Reconstructed Case, 1 = Decomposed Case
    // Since you have a decomposeParDict with 4 subdomains, we use 1.
    reader->SetCaseType(1); 
    
    // We update information first to see metadata (time steps, fields)
    reader->UpdateInformation();

    // Step 2: Set Time Step (Last one)
    int32 NumTimeSteps = reader->GetNumberOfTimeSteps();
    if (NumTimeSteps > 0)
    {
        reader->SetTimeStep(NumTimeSteps - 1);
    }

    // Optional: Enable only necessary arrays to save 1.3GB of memory
    // reader->SetPointArrayStatus("U", 1);
    // reader->SetPointArrayStatus("p", 1);

    UE_LOG(LogTemp, Log, TEXT("Executing VTK Pipeline for Decomposed Case (4 subdomains)..."));
    reader->Update();

    // Step 3: Process MultiBlock Output
    vtkMultiBlockDataSet* multiBlock = vtkMultiBlockDataSet::SafeDownCast(reader->GetOutput());
    if (!multiBlock) return nullptr;

    // Use vtkAppendPolyData to combine the 4 processor blocks into one mesh
    vtkNew<vtkAppendPolyData> appender;
    int32 BlockCount = 0;

    // OpenFOAM multiblock structure: 
    // Block 0 is usually the Internal Mesh (Unstructured Grid)
    // Subsequent blocks are patches (boundaries)
    for (unsigned int i = 0; i < multiBlock->GetNumberOfBlocks(); ++i)
    {
        vtkDataSet* block = vtkDataSet::SafeDownCast(multiBlock->GetBlock(i));
        if (!block) continue;

        // Immediately convert volume to surface to reduce 1.3GB footprint
        vtkNew<vtkDataSetSurfaceFilter> surfaceFilter;
        surfaceFilter->SetInputData(block);
        surfaceFilter->Update();

        vtkPolyData* surface = surfaceFilter->GetOutput();
        if (surface && surface->GetNumberOfPoints() > 0)
        {
            appender->AddInputData(surface);
            BlockCount++;
        }
    }

    if (BlockCount == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("No geometry blocks found in .foam file."));
        return nullptr;
    }

    UE_LOG(LogTemp, Log, TEXT("Appending %d blocks..."), BlockCount);
    appender->Update();

    // Create a persistent copy of the polydata to return
    vtkPolyData* finalResult = vtkPolyData::New();
    finalResult->DeepCopy(appender->GetOutput());

    UE_LOG(LogTemp, Log, TEXT("Foam Load Complete. Points: %lld"), (long long)finalResult->GetNumberOfPoints());

    return finalResult;
}