/**
 * Q: It can find original cell id by using  way to use the Hit location. polygon type is hexadragon has six points. But there is a problem which   highlight mesh is not appear.  what's wrong? 
 * GEM: The reason your highlight mesh is not appearing is likely due to one of three things: Coordinate Scaling, Z-Fighting, or 3D Cell Topology.

Specifically, if your original data contains Hexahedrons (3D volumes with 8 points) rather than Hexagons (2D polygons with 6 points), the "Triangle Fan" logic in the previous code will fail because it tries to treat a 3D block of points as a single flat face. Also, if you scaled your mesh (e.g., by 1000x) to see it in Unreal, the vtkCellLocator will look for the cell at the "Unreal" position (big) instead of the "VTK" position (small).

I have updated VtkCellSelectionHelper.cpp to fix the coordinate mismatch and properly handle 3D cells (Hexahedrons/Voxels) by extracting their outer faces for the highlight.
Why this fixes your issue:

    Inverse Scaling: In FindOriginalCellByPosition, I added an InverseScale. If your Unreal mesh is 1000 times larger than your VTK data, the locator won't find anything at the HitLocation unless you divide the coordinates back down to the 0.0…0.02 range first.

    vtkGenericCell and Faces: A Hexahedron (Hex) is a 3D block. You cannot simply "triangle fan" its 8 vertices because they aren't on a single flat plane. This code checks GetNumberOfFaces(). If it's a Hex, it iterates through its 6 faces and triangulates each one separately.

    Visual Scaling: In HighlightOriginalCell, I added VisualScale so that the highlight vertices are moved to the same 1000x larger position as your main mesh.

    Offsetting: I increased the offset to FaceNormal * 0.5f. If the mesh is scaled up by 1000, an offset of 0.1 is nearly invisible. 0.5 or 1.0 is safer for preventing the yellow highlight from "sinking" into the blue mesh (Z-fighting).

Recommendation: Make sure your HighlightMaterial is set to something Unlit or Emissive (like a bright Yellow) so it stands out against the shaded simulation data.

 */
#include "VtkCellSelectionHelper.h"
#include "VtkPolyDataVisualizer.h"
#include "vtkPolyData.h"
#include "vtkCellData.h"
#include "vtkTriangleFilter.h"
#include "vtkIdTypeArray.h"
#include "vtkIdList.h"
#include "vtkIdFilter.h"
#include "vtkCellLocator.h"
#include "vtkGenericCell.h" // Needed to handle different cell types (Hex, Tetra, etc.)

/**
 * 1. SPATIAL LOOKUP: Finds the original cell by looking at the 3D position of the hit.
 * Fixed: Added Inverse Scaling to match VTK's coordinate space.
 */
int32 AVtkPolyDataVisualizer::FindOriginalCellByPosition(FVector HitLocation, vtkPolyData* OriginalData)
{
    if (!OriginalData || OriginalData->GetNumberOfCells() == 0) return -1;

    // --- FIX: COORDINATE TRANSFORMATION ---
    // If you scaled your mesh by 1000 in Unreal, the HitLocation is 1000x larger 
    // than the data in the vtkCellLocator. We must scale it BACK to VTK space.
    // Replace 1000.0f with whatever ScaleFactor you used in your conversion.
    float InverseScale = 1.0f / 1000.0f; 
    FVector VtkSpacePoint = HitLocation * InverseScale;

    vtkNew<vtkCellLocator> locator;
    locator->SetDataSet(OriginalData);
    locator->BuildLocator();

    double testPoint[3] = { (double)VtkSpacePoint.X, (double)VtkSpacePoint.Y, (double)VtkSpacePoint.Z };
    double closestPoint[3];
    double dist2;
    vtkIdType cellId;
    int subId;

    locator->FindClosestPoint(testPoint, closestPoint, cellId, subId, dist2);

    return (int32)cellId;
}

/**
 * 2. HIGHLIGHT: Create an overlay mesh for the original cell.
 * Updated: Now handles 3D Cells (Hexahedrons) correctly by extracting faces.
 */
void AVtkPolyDataVisualizer::HighlightOriginalCell(int32 OriginalCellId, vtkPolyData* OriginalData)
{
    if (!OriginalData || OriginalCellId < 0 || OriginalCellId >= OriginalData->GetNumberOfCells()) return;

    // We use a GenericCell to handle any VTK type (Hexahedron, Wedge, Tetra, etc.)
    vtkNew<vtkGenericCell> cell;
    OriginalData->GetCell(OriginalCellId, cell);
    
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FLinearColor> Colors;
    TArray<FVector> Normals;

    // Use the same scale factor used for the main mesh
    float VisualScale = 1000.0f;

    // Logic: If the cell is 3D (Volume), we iterate through its FACES.
    // If it's 2D (Polygon), we just use the points.
    int32 NumFaces = cell->GetNumberOfFaces();
    
    if (NumFaces > 0)
    {
        // 3D CELL CASE (e.g. Hexahedron)
        for (int32 f = 0; f < NumFaces; f++)
        {
            vtkCell* face = cell->GetFace(f);
            int32 FaceBaseIndex = Vertices.Num();
            int32 FacePoints = face->GetNumberOfPoints();

            // Calculate Face Normal for offsetting
            double c0[3], c1[3], c2[3];
            face->GetPoints()->GetPoint(0, c0);
            face->GetPoints()->GetPoint(1, c1);
            face->GetPoints()->GetPoint(2, c2);
            FVector P0(c0[0], c0[1], c0[2]), P1(c1[0], c1[1], c1[2]), P2(c2[0], c2[1], c2[2]);
            FVector FaceNormal = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();

            for (int32 p = 0; p < FacePoints; p++)
            {
                double coords[3];
                face->GetPoints()->GetPoint(p, coords);
                // Apply Scale and a small Normal Offset to prevent Z-Fighting
                Vertices.Add((FVector(coords[0], coords[1], coords[2]) * VisualScale) + (FaceNormal * 0.5f));
                Colors.Add(FLinearColor::Yellow);
                Normals.Add(FaceNormal);
            }

            // Triangulate this face
            for (int32 i = 1; i < FacePoints - 1; i++)
            {
                Triangles.Add(FaceBaseIndex + 0);
                Triangles.Add(FaceBaseIndex + i);
                Triangles.Add(FaceBaseIndex + i + 1);
            }
        }
    }
    else
    {
        // 2D POLYGON CASE (Standard Triangle/Quad)
        int32 NumPoints = cell->GetNumberOfPoints();
        for (int32 i = 0; i < NumPoints; i++)
        {
            double coords[3];
            cell->GetPoints()->GetPoint(i, coords);
            Vertices.Add(FVector(coords[0], coords[1], coords[2]) * VisualScale);
            Colors.Add(FLinearColor::Yellow);
            Normals.Add(FVector::UpVector); // Placeholder normal
        }
        for (int32 i = 1; i < NumPoints - 1; i++)
        {
            Triangles.Add(0); Triangles.Add(i); Triangles.Add(i + 1);
        }
    }

    // Update section 10 (Selection Highlight)
    SurfaceMeshComponent->CreateMeshSection(10, Vertices, Triangles, Normals, TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);

    if (HighlightMaterial)
    {
        SurfaceMeshComponent->SetMaterial(10, HighlightMaterial);
    }
}

/**
 * 3. EXECUTION
 */
void AVtkPolyDataVisualizer::HandleMeshHitByLocation(FVector HitLocation)
{
    // Important: Pass the World/Local HitLocation from Unreal
    int32 originalId = FindOriginalCellByPosition(HitLocation, GlobalOriginalPolyData);
    
    if (originalId != -1)
    {
        HighlightOriginalCell(originalId, GlobalOriginalPolyData);
        UE_LOG(LogTemp, Log, TEXT("Picked Cell: %d"), originalId);
    }
}