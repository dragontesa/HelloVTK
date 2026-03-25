/**
dragontesa: how to convert vtk structuredpoints to polydata?
GitHub Copilot: To convert VTK `StructuredPoints` (image data) to `PolyData`, you typically use a filter like `vtkMarchingCubes` or `vtkContourFilter` to extract an isosurface, or `vtkImageDataGeometryFilter` to extract the geometry as a point cloud or surface.
Here’s a C++ example using VTK:
**/
// Example: Convert vtkStructuredPoints to vtkPolyData
// #include <vtkSmartPointer.h>
// #include <vtkStructuredPointsReader.h>
// #include <vtkImageDataGeometryFilter.h>
// #include <vtkPolyData.h>
// Example

vtkSmartPointer<vtkPolyData> StructuredPointsToPolyData(const std::string& filename)
{
    // Read the structured points file
    auto reader = vtkSmartPointer<vtkStructuredPointsReader>::New();
    reader->SetFileName(filename.c_str());
    reader->Update();

    // Convert to polydata
    auto geometryFilter = vtkSmartPointer<vtkImageDataGeometryFilter>::New();
    geometryFilter->SetInputConnection(reader->GetOutputPort());
    geometryFilter->Update();

    return geometryFilter->GetOutput();
}

vtkSmartPointer<vtkPolyData> GenerateNormals(vtkPolyData* InPolydata)
{
    // transfer attributes normals, uv, colors
    vtkNew<vtkPolyDataNormals> compute_normals;
    compute_normals->SetInputData(InPolydata);
    // optional, turnoff cell normals
    compute_normals->ComputePointNormalsOn(); // Needed for UE (vertex normals)
    compute_normals->ComputeCellNormalsOff(); // Usally not need for UE
    compute_normals->SplittingOff(); // prevents hard edges unless needed
    compute_normals->ConsistencyOn();
    compute_normals->AutoOrientNormalsOn();
    compute_normals->Update();

    vtkSmartPointer<vtkPolyData> outPolyData = compute_normals->GetOutput();
    return outPolyData;
}

/**

- Use `vtkImageDataGeometryFilter` for extracting geometry (points/surfaces).
- Use `vtkMarchingCubes` or `vtkContourFilter` for extracting isosurfaces (e.g., for volume rendering).

**Choose the filter based on your use case:**
- For a point cloud or surface: `vtkImageDataGeometryFilter`
- For an isosurface: `vtkMarchingCubes` or `vtkContourFilter`

Let me know if you need an Unreal Engine integration example or more details!

 */
