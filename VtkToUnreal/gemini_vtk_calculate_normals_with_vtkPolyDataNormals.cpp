#include <vtkPolyDataNormals.h>
#include <vtkTriangleFilter.h> // Useful if your polygons aren't all triangles
#include <vtkPointData.h>
#include <vtkDataArray.h> // Base class for normals array

// ... (your includes for reader, polydata, etc.) ...

// Assuming 'polydata' is your vtkPolyData object obtained from vtkPolyDataReader->GetOutput()

// --- Step 1: Ensure the mesh is triangulated (if necessary) ---
// vtkPolyDataNormals works best with triangular meshes.
// If your VTK file contains quads (like the cube example) or other polygons,
// it's often a good idea to triangulate them first.
vtkNew<vtkTriangleFilter> triangleFilter;
triangleFilter->SetInputData(polydata);
triangleFilter->Update();
vtkSmartPointer<vtkPolyData> triangulatedPolyData = triangleFilter->GetOutput();

// --- Step 2: Compute Normals using vtkPolyDataNormals ---
vtkNew<vtkPolyDataNormals> normalsFilter;
normalsFilter->SetInputData(triangulatedPolyData); // Use the triangulated data

// Key properties to set:
normalsFilter->SetComputePointNormals(true);  // Compute smooth vertex normals (usually desired)
normalsFilter->SetComputeCellNormals(false); // Don't compute face normals (unless explicitly needed)
normalsFilter->SetAutoOrientNormals(true);    // Tries to orient all normals consistently outwards
normalsFilter->SetSplitting(false);          // Don't split edges based on feature angle for hard edges.
                                              // Set to 'true' if you want hard edges at sharp angles.
                                              // normalsFilter->SetFeatureAngle(30.0); // Only for SetSplitting(true)

// Execute the filter
normalsFilter->Update();

// Get the polydata with computed normals
vtkSmartPointer<vtkPolyData> polyDataWithNormals = normalsFilter->GetOutput();

// --- Step 3: Access the Computed Normals ---
// The computed point normals are stored in the PointData of the output polydata
vtkDataArray* normalsArray = polyDataWithNormals->GetPointData()->GetNormals();

if (!normalsArray) {
    // This should ideally not happen if normalsFilter->Update() was successful and SetComputePointNormals(true)
    UE_LOG(LogTemp, Error, TEXT("VTK: Failed to compute or retrieve point normals!"));
    // Handle error, perhaps use a default normal if this is critical.
} else {
    UE_LOG(LogTemp, Log, TEXT("VTK: Successfully computed and retrieved %lld point normals."), normalsArray->GetNumberOfTuples());

    // You can now iterate through normalsArray to get the (x,y,z) components
    // for each point and transfer them to Unreal.
    // Each tuple in normalsArray corresponds to a point in polyDataWithNormals->GetPoints().
    for (vtkIdType i = 0; i < normalsArray->GetNumberOfTuples(); ++i) {
        double normal[3];
        normalsArray->GetTuple(i, normal); // Get (x,y,z) components for normal 'i'
        // UE_LOG(LogTemp, Verbose, TEXT("Normal[%lld]: X=%f, Y=%f, Z=%f"), i, normal[0], normal[1], normal[2]);
    }
}

// polyDataWithNormals is now the polydata you'll use to extract vertices, triangles, and normals.