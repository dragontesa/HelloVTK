/**
 * Q: I want to find the cell which matched to the face index get traced on unreal mesh in the original polydata
 *  before converting triangle poly.
 *  Then I get points in the cell and generate a overlay mesh with that cell's points. 
 * GEM: This is a classic "Picking" problem in scientific visualization. To solve this, 
 * we need to track the Pedigree ID of the cells during the triangulation process.
 * If you use vtkTriangleFilter to convert your original data (with quads/N-gons) into triangles, 
 * VTK can automatically generate an array that maps each new triangle back to its parent cell ID.
 * We then store this in a mapping array for Unreal.
 * I've created a new helper file that implements the Selection Pipeline: 
 * it triangulates while preserving IDs, maps the Unreal Hit, and generates a "Highlight Overlay" for the * original N-gon cell.
 * 
 * How this workflow works:
    Preparation (PrepareTriangulatedDataWithIDs):
        You input your raw Foam data (with Quads/Hexes).
        vtkTriangleFilter splits those into triangles.
        By enabling SetPassThroughCellIds(1), VTK adds an attribute to the triangles that says: "I am triangle #500, but I came from original Quad #200".
        We save this mapping into OutFaceToOriginalCellMap.

    Conversion:
        You convert the triangulated output to Unreal exactly as you did before.

        The Hit (HandleMeshHit):
        When you do a LineTraceByChannel, Unreal gives you FaceIndex = 500.
        You look at your map: FaceToOriginalCellMap[500] → 200.

        The Highlight (HighlightOriginalCell):
        We go back to the original, non-triangulated data.
        We find Cell #200 (which might be a Quad).
        We extract its 4 points, add a tiny bit of "Offset" 
        (to push it slightly in front of the surface so it doesn't flicker), 
        and render it in a bright color on a new section index.

    Q: First thing, If I have to compute vtkQuadricClustering filter to reduce large polygons, 
     how to get a original cell map? Does this filter output triangle poly data?  
    GEM: Gemini의 응답
    I will update the Canvas to handle the complexities introduced by decimation filters like vtkQuadricClustering.
    Regarding your questions:
    Does it output triangles? Yes, vtkQuadricClustering outputs vtkPolyData. 
    While it usually generates triangles, the output can occasionally contain non-triangular polygons 
    depending on how the grid cells are merged. 
    It is always safest to run a vtkTriangleFilter after clustering if your Unreal conversion expects triangles.
    How to get the original cell map? vtkQuadricClustering is a "lossy" filter — 
    it physically removes original geometry and creates new, averaged cells. 
    Unlike the TriangleFilter, it does not have a 1-to-1 or N-to-1 "Pedigree" mapping.

    To solve this, the standard approach in scientific visualization is Spatial Searching. 
    We perform the Line Trace on the decimated Unreal mesh, get the coordinate of the hit, 
    and then use a vtkCellLocator on the original high-res data to find which cell is physically closest to that spot.

    I have updated the Canvas to use a vtkCellLocator. Because vtkQuadricClustering creates entirely new points 
    and triangles that do not share IDs with the original mesh, the FaceToOriginalCellMap approach will not work.
    Instead, FindOriginalCellByPosition takes the 3D coordinate from your FHitResult.
    Location and finds the mathematically closest cell in the original data. 
    This ensures your "Highlight" always matches the high-resolution source perfectly.

 */
#include "VtkPolyDataVisualizer.h"
#include "vtkPolyData.h"
#include "vtkCellData.h"
#include "vtkTriangleFilter.h"
#include "vtkIdTypeArray.h"
#include "vtkIdList.h"
#include "vtkQuadricClustering.h"
#include "vtkCellLocator.h"
#include "vtkPointData.h"

/**
 * 1. PRE-PROCESS: Decimate data and then triangulate.
 * Note: Decimation (Clustering) destroys direct ID mapping, so we rely on 
 * spatial lookups later instead of a simple TArray map.
 */
vtkPolyData* AVtkPolyDataVisualizer::PrepareDecimatedData(vtkDataSet* OriginalData)
{
    if (!OriginalData) return nullptr;

    // A. Quadric Clustering to reduce large datasets (e.g., your 1.3GB file)
    vtkNew<vtkQuadricClustering> cluster;
    cluster->SetInputData(OriginalData);
    cluster->SetNumberOfXDivisions(60);
    cluster->SetNumberOfYDivisions(60);
    cluster->SetNumberOfZDivisions(60);
    cluster->Update();

    // B. Triangulate the result to ensure Unreal gets 3-point faces
    vtkNew<vtkTriangleFilter> triangulator;
    triangulator->SetInputConnection(cluster->GetOutputPort());
    triangulator->Update();

    return triangulator->GetOutput();
}

/**
 * 2. SPATIAL LOOKUP: Finds the original cell by looking at the 3D position of the hit.
 * This is the ONLY way to map hits back to original data after using Quadric Clustering.
 */
int32 AVtkPolyDataVisualizer::FindOriginalCellByPosition(FVector HitLocation, vtkPolyData* OriginalData)
{
    if (!OriginalData || OriginalData->GetNumberOfCells() == 0) return -1;

    // Use a Cell Locator for fast 3D spatial searching
    vtkNew<vtkCellLocator> locator;
    locator->SetDataSet(OriginalData);
    locator->BuildLocator();

    double testPoint[3] = { HitLocation.X, HitLocation.Y, HitLocation.Z };
    double closestPoint[3];
    double dist2;
    vtkIdType cellId;
    int subId;

    // Find the cell containing or closest to the hit location
    locator->FindClosestPoint(testPoint, closestPoint, cellId, subId, dist2);

    return (int32)cellId;
}

/**
 * 3. HIGHLIGHT: Create an overlay mesh for the original cell
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

    FVector P0, P1, P2;
    double coords[3];
    
    OriginalData->GetPoint(cellPointIds->GetId(0), coords);
    P0 = FVector(coords[0], coords[1], coords[2]);
    OriginalData->GetPoint(cellPointIds->GetId(1), coords);
    P1 = FVector(coords[0], coords[1], coords[2]);
    OriginalData->GetPoint(cellPointIds->GetId(2), coords);
    P2 = FVector(coords[0], coords[1], coords[2]);

    FVector FaceNormal = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
    FVector Offset = FaceNormal * 0.1f; 

    for (int32 i = 0; i < numPoints; i++)
    {
        OriginalData->GetPoint(cellPointIds->GetId(i), coords);
        Vertices.Add(FVector(coords[0], coords[1], coords[2]) + Offset);
        Colors.Add(FLinearColor::Yellow);
        Normals.Add(FaceNormal);
    }
 
    for (int32 i = 1; i < numPoints - 1; i++)
    {
        Triangles.Add(0);
        Triangles.Add(i);
        Triangles.Add(i + 1);
    }

    SurfaceMeshComponent->CreateMeshSection(
        10, 
        Vertices, 
        Triangles, 
        Normals, 
        TArray<FVector2D>(), 
        Colors, 
        TArray<FProcMeshTangent>(), 
        false
    );

    if (HighlightMaterial)
    {
        SurfaceMeshComponent->SetMaterial(10, HighlightMaterial);
    }
}

/**
 * 4. EXECUTION: Triggered by Unreal Line Trace
 */
void AVtkPolyDataVisualizer::HandleMeshHitByLocation(FVector HitLocation)
{
    // Since we used Quadric Clustering, we search the original high-res data
    // to find which cell is at this specific 3D coordinate.
    int32 originalId = FindOriginalCellByPosition(HitLocation, GlobalOriginalPolyData);
    
    if (originalId != -1)
    {
        HighlightOriginalCell(originalId, GlobalOriginalPolyData);
        UE_LOG(LogTemp, Log, TEXT("Spatial Search found Original Cell: %d"), originalId);
    }
}