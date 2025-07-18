#include <vtkPolyData.h>
#include <vtkPointData.h>
#include <vtkDataArray.h> // For accessing texture coordinates (UVs)
#include "KismetProceduralMeshLibrary.h" // For UKismetProceduralMeshLibrary

// ... (inside your AMyVtkMeshActor::LoadVtkMeshAndCreateProceduralMesh function) ...

// After you have polyDataWithNormals and extracted Vertices, Triangles, and Normals:

// --- Extract UVs (if available in VTK data) ---
TArray<FVector2D> UV0;
vtkDataArray* vtkTCoords = polyDataWithNormals->GetPointData()->GetTCoords(); // Get Texture Coordinates

if (vtkTCoords && vtkTCoords->GetNumberOfComponents() >= 2) { // UVs usually have 2 components (U, V)
    UV0.Reserve(vtkTCoords->GetNumberOfTuples());
    for (vtkIdType i = 0; i < vtkTCoords->GetNumberOfTuples(); ++i) {
        double uv[2]; // Or uv[3] if it's 3D texture coords, but UPMC expects 2D
        vtkTCoords->GetTuple(i, uv);
        UV0.Add(FVector2D(uv[0], uv[1]));
    }
    UE_LOG(LogTemp, Log, TEXT("Extracted %d UV coordinates."), UV0.Num());
} else {
    UE_LOG(LogTemp, Warning, TEXT("VTK: No valid UV coordinates found. Tangents might not be accurate or cannot be computed by Unreal."));
    // Fill with dummy UVs if none are found, to avoid crashes.
    UV0.Init(FVector2D::ZeroVector, Vertices.Num());
}

// ... (other data like VertexColors, Tangents) ...
TArray<FLinearColor> VertexColors; // Keep as placeholder for now, or populate from VTK scalars
TArray<FProcMeshTangent> Tangents; // This will be filled by the library

// Create the mesh section
ProcMesh->CreateMeshSection(
    0,
    Vertices,
    Triangles,
    Normals,
    UV0,
    VertexColors,
    Tangents, // Pass the empty Tangents array
    true
);

// --- AFTER CreateMeshSection, calculate tangents using Unreal's library ---
// This function needs the mesh data that's *already* in the ProcMeshComponent
UKismetProceduralMeshLibrary::CalculateTangentsForMesh(
    Vertices,
    Triangles,
    Normals,
    UV0,
    Tangents // Output tangents
);

// Now, update the mesh section with the computed tangents
ProcMesh->UpdateMeshSection(
    0, // Section Index
    Vertices,
    Normals,
    UV0,
    VertexColors,
    Tangents
);

// Note: You *must* re-call UpdateMeshSection if you modify the mesh data after CreateMeshSection.
// For initial creation, you could technically calculate them before CreateMeshSection
// and pass the `Tangents` directly. However, CalculateTangentsForMesh usually
// works best *after* the initial section is created, as it often expects the
// vertex/triangle indices to be finalized.