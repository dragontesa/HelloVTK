/**
 * Q: There is strange points. Any found cell id is same id even hit location (or impact point) coords are different. why? 
 * GEM: The reason you are getting the same Cell ID even when the hit locations are different is almost certainly because the vtkCellLocator is searching in the wrong coordinate space.

In Unreal, the HitLocation (Impact Point) is typically in World Space. However, VTK data is loaded in its own Local Space (usually starting at 0,0,0). If your Actor is moved, rotated, or scaled in the Unreal level, the World coordinates will not match the VTK coordinates, causing the locator to always snap to the "closest" point on the boundary of the dataset, which often ends up being the same corner cell for every hit.

I have updated VtkCellSelectionHelper.cpp to properly transform the hit location from Unreal World Space to VTK Local Space before performing the search.
Why this fixes the "Same ID" problem:

    InverseTransformPosition(HitLocation): This converts the World Hit (e.g., X=5000 in your Unreal Level) into the Actor's Local space (X=10 relative to the mesh pivot). Without this, if you move your actor even a few centimeters, the VTK search point will fly way outside the dataset bounds.

    Scale Alignment: By ensuring InverseScale perfectly counters your VisualScale, the point passed to VTK will be in the original 0.0…0.02 range where your hexahedrons exist.

    Distance Logging: I added a distance check. If the log says "Hit is far from VTK data," it confirms that your transformation math (Scaling or Pivot) is still slightly off.

Note: If your OriginalData is a vtkUnstructuredGrid from the OpenFOAM reader, ensure you are passing the Volume data to the locator, not just a surface-filtered version, to get the most accurate 3D cell matching.
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
#include "vtkGenericCell.h"

/**
 * 1. SPATIAL LOOKUP: Finds the original cell by looking at the 3D position of the hit.
 * Fix: Added Actor Transformation to convert World Space hits to VTK Local Space.
 */
int32 AVtkPolyDataVisualizer::FindOriginalCellByPosition(FVector HitLocation, vtkPolyData* OriginalData)
{
    if (!OriginalData || OriginalData->GetNumberOfCells() == 0) return -1;

    // --- CRITICAL FIX 1: WORLD TO LOCAL TRANSFORMATION ---
    // Unreal Line Traces return World Space coordinates. 
    // We must convert this to the Actor's Local Space first.
    FVector LocalHit = GetActorTransform().InverseTransformPosition(HitLocation);

    // --- CRITICAL FIX 2: APPLY INVERSE SCALE ---
    // If you scaled your mesh by 1000 in Unreal, the LocalHit is 1000x larger 
    // than the data in VTK. We must scale it back down.
    // Use the same scale factor you used during conversion.
    float InverseScale = 1.0f / 1000.0f; 
    FVector VtkSpacePoint = LocalHit * InverseScale;

    // Setup Locator
    vtkNew<vtkCellLocator> locator;
    locator->SetDataSet(OriginalData);
    locator->BuildLocator();

    double testPoint[3] = { (double)VtkSpacePoint.X, (double)VtkSpacePoint.Y, (double)VtkSpacePoint.Z };
    double closestPoint[3];
    double dist2;
    vtkIdType cellId;
    int subId;

    // Search
    locator->FindClosestPoint(testPoint, closestPoint, cellId, subId, dist2);

    // Optional: Log distance to see if we are actually close to the data
    // If dist2 is very large, the hit point is outside the VTK bounding box.
    if (FMath::Sqrt(dist2) > 0.1) // Adjust threshold based on your cell size
    {
        UE_LOG(LogTemp, Warning, TEXT("Hit is far from VTK data. Dist: %f"), FMath::Sqrt(dist2));
    }

    return (int32)cellId;
}

/**
 * 2. HIGHLIGHT: Create an overlay mesh for the original cell.
 */
void AVtkPolyDataVisualizer::HighlightOriginalCell(int32 OriginalCellId, vtkPolyData* OriginalData)
{
    if (!OriginalData || OriginalCellId < 0 || OriginalCellId >= OriginalData->GetNumberOfCells()) return;

    vtkNew<vtkGenericCell> cell;
    OriginalData->GetCell(OriginalCellId, cell);
    
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FLinearColor> Colors;
    TArray<FVector> Normals;

    float VisualScale = 1000.0f;
    int32 NumFaces = cell->GetNumberOfFaces();
    
    if (NumFaces > 0)
    {
        // 3D Volume Cell (Hexahedron, etc.)
        for (int32 f = 0; f < NumFaces; f++)
        {
            vtkCell* face = cell->GetFace(f);
            int32 FaceBaseIndex = Vertices.Num();
            int32 FacePoints = face->GetNumberOfPoints();

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
                // Apply Scale and Offset along Normal to prevent Z-Fighting
                Vertices.Add((FVector(coords[0], coords[1], coords[2]) * VisualScale) + (FaceNormal * 0.2f));
                Colors.Add(FLinearColor::Yellow);
                Normals.Add(FaceNormal);
            }

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
        // 2D Polygon Cell
        int32 NumPoints = cell->GetNumberOfPoints();
        for (int32 i = 0; i < NumPoints; i++)
        {
            double coords[3];
            cell->GetPoints()->GetPoint(i, coords);
            Vertices.Add(FVector(coords[0], coords[1], coords[2]) * VisualScale);
            Colors.Add(FLinearColor::Yellow);
            Normals.Add(FVector::UpVector);
        }
        for (int32 i = 1; i < NumPoints - 1; i++)
        {
            Triangles.Add(0); Triangles.Add(i); Triangles.Add(i + 1);
        }
    }

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
    int32 originalId = FindOriginalCellByPosition(HitLocation, GlobalOriginalPolyData);
    
    if (originalId != -1)
    {
        HighlightOriginalCell(originalId, GlobalOriginalPolyData);
        UE_LOG(LogTemp, Log, TEXT("Picked Cell: %d at Hit %s"), originalId, *HitLocation.ToString());
    }
}


/**
 * Q: Yes, My OriginalData is vtkUnstructuredGrid but I don't pass Volume data to the locator. how do it? 
 * GEM: o find the 3D cell in your original volume data (the vtkUnstructuredGrid), you need to ensure you are storing a reference to the data before it passes through the vtkDataSetSurfaceFilter.

When you use the Surface Filter, you throw away the "insides" of the volume to create a shell. For selection to work accurately—especially if you want to identify Hexahedrons or Tetrahedrons—the vtkCellLocator must be built using the full vtkUnstructuredGrid.

I have updated VtkCellSelectionHelper.cpp to accept the generic vtkDataSet* type. This allows you to pass your high-resolution volume data (Unstructured Grid) directly to the locator and the highlighting logic.
How to use this with your OpenFOAM data:

    Keep the Volume Pointer: In your FoamDataReader.cpp, when you get multiBlock->GetBlock(0), save that as a variable (e.g., vtkUnstructuredGrid* InternalVolumeData) before you pass it to the vtkDataSetSurfaceFilter.

    Pass to Helper: When the user clicks on the mesh in Unreal, take the HitResult.ImpactPoint and call:
    HandleVolumePicking(Hit.ImpactPoint, InternalVolumeData);

    Why this works: The vtkCellLocator will now search the entire 3D grid. Even though the "Hit" happens on the surface, the locator will find the Hexahedron that contains that surface face. The HighlightOriginalCell then extracts all 6 faces of that Hexahedron and draws them as a yellow shell over the original cell.

Note: If your highlight still doesn't appear, check that your HighlightMaterial is set to be Two-Sided and preferably Unlit/Emissive so it is visible even if it's slightly inside the mesh.
 */

 #include "VtkCellSelectionHelper.h"
#include "VtkPolyDataVisualizer.h"
#include "vtkDataSet.h"      // Changed from vtkPolyData to vtkDataSet for Volume support
#include "vtkUnstructuredGrid.h"
#include "vtkCellData.h"
#include "vtkTriangleFilter.h"
#include "vtkIdTypeArray.h"
#include "vtkIdList.h"
#include "vtkCellLocator.h"
#include "vtkGenericCell.h"

/**
 * 1. SPATIAL LOOKUP: Finds the original 3D cell in the Volume data.
 * @param OriginalData Pass the vtkUnstructuredGrid (Volume) here.
 */
int32 AVtkPolyDataVisualizer::FindOriginalCellByPosition(FVector HitLocation, vtkDataSet* OriginalData)
{
    if (!OriginalData || OriginalData->GetNumberOfCells() == 0) return -1;

    // --- COORDINATE TRANSFORMATION ---
    // 1. Convert World Hit to Actor Local Space
    FVector LocalHit = GetActorTransform().InverseTransformPosition(HitLocation);

    // 2. Scale back to VTK original units (e.g. 0.02 range)
    float InverseScale = 1.0f / 1000.0f; 
    FVector VtkSpacePoint = LocalHit * InverseScale;

    // --- VOLUME LOCATOR ---
    // vtkCellLocator works on vtkUnstructuredGrid (Volumes) and vtkPolyData (Surfaces)
    vtkNew<vtkCellLocator> locator;
    locator->SetDataSet(OriginalData);
    locator->BuildLocator();

    double testPoint[3] = { (double)VtkSpacePoint.X, (double)VtkSpacePoint.Y, (double)VtkSpacePoint.Z };
    double closestPoint[3];
    double dist2;
    vtkIdType cellId;
    int subId;

    // This finds the closest 3D cell in the volume to the point on the Unreal surface
    locator->FindClosestPoint(testPoint, closestPoint, cellId, subId, dist2);

    return (int32)cellId;
}

/**
 * 2. HIGHLIGHT: Extracts the boundary faces of a 3D Volume Cell (e.g. Hexahedron).
 */
void AVtkPolyDataVisualizer::HighlightOriginalCell(int32 OriginalCellId, vtkDataSet* OriginalData)
{
    if (!OriginalData || OriginalCellId < 0 || OriginalCellId >= OriginalData->GetNumberOfCells()) return;

    // Get the cell (VTK handles if it's a Hex, Tetra, or Wedge automatically)
    vtkNew<vtkGenericCell> cell;
    OriginalData->GetCell(OriginalCellId, cell);
    
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FLinearColor> Colors;
    TArray<FVector> Normals;

    float VisualScale = 1000.0f;
    int32 NumFaces = cell->GetNumberOfFaces();
    
    // If NumFaces > 0, it's a 3D Volume Cell (like a Hexahedron from OpenFOAM)
    if (NumFaces > 0)
    {
        for (int32 f = 0; f < NumFaces; f++)
        {
            vtkCell* face = cell->GetFace(f);
            int32 FaceBaseIndex = Vertices.Num();
            int32 FacePoints = face->GetNumberOfPoints();

            // Calculate Normal for this specific face of the Hex
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
                // Position + Small Normal Offset to sit "on top" of the volume
                Vertices.Add((FVector(coords[0], coords[1], coords[2]) * VisualScale) + (FaceNormal * 0.2f));
                Colors.Add(FLinearColor::Yellow);
                Normals.Add(FaceNormal);
            }

            // Triangulate the face (handles quads or higher polygons)
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
        // Fallback for 2D cells
        int32 NumPoints = cell->GetNumberOfPoints();
        for (int32 i = 0; i < NumPoints; i++)
        {
            double coords[3];
            cell->GetPoints()->GetPoint(i, coords);
            Vertices.Add(FVector(coords[0], coords[1], coords[2]) * VisualScale);
            Colors.Add(FLinearColor::Yellow);
            Normals.Add(FVector::UpVector);
        }
        for (int32 i = 1; i < NumPoints - 1; i++)
        {
            Triangles.Add(0); Triangles.Add(i); Triangles.Add(i + 1);
        }
    }

    // Section 10 for the highlight
    SurfaceMeshComponent->CreateMeshSection(10, Vertices, Triangles, Normals, TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);

    if (HighlightMaterial)
    {
        SurfaceMeshComponent->SetMaterial(10, HighlightMaterial);
    }
}

/**
 * 3. EXECUTION: Called from your Picking logic
 * @param VolumeData The vtkUnstructuredGrid you want to pick from.
 */
void AVtkPolyDataVisualizer::HandleVolumePicking(FVector HitLocation, vtkDataSet* VolumeData)
{
    int32 pickedCellId = FindOriginalCellByPosition(HitLocation, VolumeData);
    
    if (pickedCellId != -1)
    {
        HighlightOriginalCell(pickedCellId, VolumeData);
        UE_LOG(LogTemp, Log, TEXT("Successfully picked 3D Volume Cell: %d"), pickedCellId);
    }
}