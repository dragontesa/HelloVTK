#include "VtkPolyDataVisualizer.h"
#include "vtkPolyData.h"
#include "vtkCellData.h"
#include "vtkTriangleFilter.h"
#include "vtkIdTypeArray.h"
#include "vtkIdList.h"
#include "vtkIdFilter.h" // Added to generate IDs manually

/**
 * 1. PRE-PROCESS: Triangulate the original data while preserving Original Cell IDs.
 * Fixed: Replaced non-existent SetPassThroughCellIds with vtkIdFilter pipeline.
 */
vtkPolyData* AVtkPolyDataVisualizer::PrepareTriangulatedDataWithIDs(vtkPolyData* OriginalData, TArray<int32>& OutFaceToOriginalCellMap)
{
    if (!OriginalData) return nullptr;

    // STEP A: Generate explicit IDs on the original cells
    // This is the standard way to track "Pedigree" in VTK across filters
    vtkNew<vtkIdFilter> idFilter;
    idFilter->SetInputData(OriginalData);
    idFilter->SetCellIdsArrayName("vtkOriginalCellIds");
    idFilter->CellIdsOn();   // Generate IDs for Cells
    idFilter->PointIdsOff(); // We only care about Cells for FaceIndex mapping
    idFilter->Update();

    // STEP B: Triangulate the data
    // Filters like vtkTriangleFilter preserve Cell Data (including our new ID array) by default
    vtkNew<vtkTriangleFilter> triangulator;
    triangulator->SetInputConnection(idFilter->GetOutputPort());
    triangulator->Update();

    vtkPolyData* triangulatedOutput = triangulator->GetOutput();
    
    // STEP C: Extract the mapping array from the output
    vtkIdTypeArray* originalIds = vtkIdTypeArray::SafeDownCast(
        triangulatedOutput->GetCellData()->GetAbstractArray("vtkOriginalCellIds")
    );

    if (originalIds)
    {
        OutFaceToOriginalCellMap.Empty();
        for (vtkIdType i = 0; i < originalIds->GetNumberOfValues(); i++)
        {
            // Each triangle in the output now carries the ID of its parent cell
            OutFaceToOriginalCellMap.Add((int32)originalIds->GetValue(i));
        }
    }

    return triangulatedOutput;
}

/**
 * 2. HIGHLIGHT: Create an overlay mesh for the original cell (even if it was a Quad or N-gon)
 * @param OriginalCellId The ID of the cell in the ORIGINAL (non-triangulated) polydata
 * @param OriginalData The original polydata containing the Quads/N-gons
 */
void AVtkPolyDataVisualizer::HighlightOriginalCell(int32 OriginalCellId, vtkPolyData* OriginalData)
{
    if (!OriginalData || OriginalCellId < 0 || OriginalCellId >= OriginalData->GetNumberOfCells()) return;

    vtkNew<vtkIdList> cellPointIds;
    OriginalData->GetCellPoints(OriginalCellId, cellPointIds);
    
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FLinearColor> Colors;
    TArray<FVector> Normals;

    int32 numPoints = cellPointIds->GetNumberOfIds();
    if (numPoints < 3) return;

    // Extract the points and calculate a face normal for offsetting
    FVector P0, P1, P2;
    double coords[3];
    
    OriginalData->GetPoint(cellPointIds->GetId(0), coords);
    P0 = FVector(coords[0], coords[1], coords[2]);
    OriginalData->GetPoint(cellPointIds->GetId(1), coords);
    P1 = FVector(coords[0], coords[1], coords[2]);
    OriginalData->GetPoint(cellPointIds->GetId(2), coords);
    P2 = FVector(coords[0], coords[1], coords[2]);

    FVector FaceNormal = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
    
    // Small offset to prevent Z-Fighting (flickering) with the main mesh
    FVector Offset = FaceNormal * 0.1f; 

    // Add vertices for the whole N-gon
    for (int32 i = 0; i < numPoints; i++)
    {
        OriginalData->GetPoint(cellPointIds->GetId(i), coords);
        Vertices.Add(FVector(coords[0], coords[1], coords[2]) + Offset);
        Colors.Add(FLinearColor::Yellow); // Highlight color
        Normals.Add(FaceNormal);
    }

    // Triangulate the N-gon for the overlay (Triangle Fan)
    for (int32 i = 1; i < numPoints - 1; i++)
    {
        Triangles.Add(0);
        Triangles.Add(i);
        Triangles.Add(i + 1);
    }

    // Create the Highlight Section (Section 10 is reserved for selection)
    SurfaceMeshComponent->CreateMeshSection(
        10, 
        Vertices, 
        Triangles, 
        Normals, 
        TArray<FVector2D>(), 
        Colors, 
        TArray<FProcMeshTangent>(), 
        false // No collision for overlay
    );

    // Use a special highlight material (e.g., Unlit yellow or Emissive)
    if (HighlightMaterial)
    {
        SurfaceMeshComponent->SetMaterial(10, HighlightMaterial);
    }
}

/**
 * 3. EXECUTION: The workflow triggered by a Line Trace hit
 */
void AVtkPolyDataVisualizer::HandleMeshHit(int32 FaceIndex)
{
    // 1. Get the Original Cell ID using our map
    if (FaceToOriginalCellMap.IsValidIndex(FaceIndex))
    {
        int32 originalId = FaceToOriginalCellMap[FaceIndex];
        
        // 2. Generate the overlay using the original N-gon data
        HighlightOriginalCell(originalId, GlobalOriginalPolyData);
        
        UE_LOG(LogTemp, Log, TEXT("Highlighted Original Cell: %d"), originalId);
    }
}