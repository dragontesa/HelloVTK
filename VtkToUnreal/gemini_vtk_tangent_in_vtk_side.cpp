#include <vtkPolyDataTangents.h>
#include <vtkPointData.h> // Tangents are stored in PointData
#include <vtkFloatArray.h> // Tangents are usually float vectors

// ... (after vtkPolyDataNormals::Update(), using polyDataWithNormals) ...
/**
 * You can also compute tangents directly in VTK using vtkPolyDataTangents. 
 * This also requires UV coordinates to be present in your vtkPolyData.
 */
// --- Check for existing UVs (Texture Coordinates) in the PolyData ---
vtkDataArray* inputTCoords = polyDataWithNormals->GetPointData()->GetTCoords();
if (!inputTCoords || inputTCoords->GetNumberOfComponents() < 2) {
    UE_LOG(LogTemp, Warning, TEXT("VTK: No valid UV coordinates (TextureCoords) found for tangent computation. Skipping vtkPolyDataTangents."));
    // You'll have to use Unreal's method or generate dummy UVs if tangents are critical.
    // If you proceed, the tangents array will be null or empty.
} else {
    // --- Compute Tangents using vtkPolyDataTangents ---
    vtkNew<vtkPolyDataTangents> tangentsFilter;
    tangentsFilter->SetInputData(polyDataWithNormals); // Input should have vertices, triangles, normals, and UVs
    tangentsFilter->Update();

    vtkSmartPointer<vtkPolyData> polyDataWithTangents = tangentsFilter->GetOutput();

    vtkDataArray* vtkTangents = polyDataWithTangents->GetPointData()->GetTangents();
    if (!vtkTangents) {
        UE_LOG(LogTemp, Error, TEXT("VTK: Failed to compute or retrieve tangents!"));
    } else {
        UE_LOG(LogTemp, Log, TEXT("VTK: Successfully computed and retrieved %lld tangents."), vtkTangents->GetNumberOfTuples());

        // --- Extract Tangents for Unreal ---
        TArray<FProcMeshTangent> Tangents;
        Tangents.Reserve(vtkTangents->GetNumberOfTuples());
        for (vtkIdType i = 0; i < vtkTangents->GetNumberOfTuples(); ++i) {
            double t[3];
            vtkTangents->GetTuple(i, t);
            // Tangents from VTK are usually just the X-direction of the tangent space.
            // Unreal's FProcMeshTangent takes Normal (Z-axis), Tangent (X-axis), and determines Bitangent (Y-axis) and Handedness.
            // The W component (OrthogonalBinormal) is usually True (1) if the normal, tangent, bitangent form a right-handed system.
            // You might need to adjust the W component if the tangent basis results in a left-handed system (uncommon for correctly computed tangents).
            Tangents.Add(FProcMeshTangent(t[0], t[1], t[2])); // Just pass X,Y,Z of tangent vector. W is often 1.
        }
        UE_LOG(LogTemp, Log, TEXT("Extracted %d tangents for Unreal."), Tangents.Num());

        // Now pass these 'Tangents' to CreateMeshSection/UpdateMeshSection
        // ...
    }
}