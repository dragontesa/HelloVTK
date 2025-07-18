#include <vtkPolyDataReader.h> // Ensure this is vtkPolyDataReader, not vtkPolyData
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkDataArray.h> // For GetData()

// ... inside your Unreal function/method ...

vtkNew<vtkPolyDataReader> reader; // Corrected type
FString VtkFilePath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("my_polydata.vtk"));
reader->SetFileName(TCHAR_TO_ANSI(*VtkFilePath)); // Convert FString to const char*
reader->Update();

// It's good practice to assign to vtkSmartPointer immediately for proper ref counting
vtkSmartPointer<vtkPolyData> poly = reader->GetOutput();

if (!poly)
{
    UE_LOG(LogTemp, Error, TEXT("VTK: reader->GetOutput() returned NULL PolyData!"));
    return;
}

// Basic checks on the polydata object itself
UE_LOG(LogTemp, Log, TEXT("VTK: PolyData Ref Count: %d"), poly->GetReferenceCount()); // Should be >= 1
UE_LOG(LogTemp, Log, TEXT("VTK: PolyData Number of Points: %lld"), poly->GetNumberOfPoints());
UE_LOG(LogTemp, Log, TEXT("VTK: PolyData Number of Cells: %lld"), poly->GetNumberOfCells());

vtkPoints* points = poly->GetPoints();
if (!points)
{
    UE_LOG(LogTemp, Error, TEXT("VTK: poly->GetPoints() returned NULL!"));
    return;
}
UE_LOG(LogTemp, Log, TEXT("VTK: Points data exists: %s"), (points->GetData() ? TEXT("true") : TEXT("false")));
// Try to access number of tuples from the raw data array
if (points->GetData()) {
    UE_LOG(LogTemp, Log, TEXT("VTK: Points data array number of tuples: %lld"), points->GetData()->GetNumberOfTuples());
} else {
    UE_LOG(LogTemp, Error, TEXT("VTK: points->GetData() returned NULL data array!"));
}


vtkCellData* cellData = poly->GetCellData();
if (!cellData)
{
    UE_LOG(LogTemp, Error, TEXT("VTK: poly->GetCellData() returned NULL!"));
    return;
}

vtkDataArray* cellNormals = cellData->GetNormals(); // This is where it crashes
if (!cellNormals)
{
    UE_LOG(LogTemp, Warning, TEXT("VTK: cellData->GetNormals() returned NULL (no normals found or valid)!"));
} else {
    UE_LOG(LogTemp, Log, TEXT("VTK: Cell Normals array exists: %s"), (cellNormals->GetDataPointer() ? TEXT("true") : TEXT("false")));
    if (cellNormals->GetDataPointer()) {
        UE_LOG(LogTemp, Log, TEXT("VTK: Cell Normals number of tuples: %lld"), cellNormals->GetNumberOfTuples());
    } else {
         UE_LOG(LogTemp, Error, TEXT("VTK: cellNormals->GetDataPointer() returned NULL pointer!"));
    }
}

// ... rest of your code ...