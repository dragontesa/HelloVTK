// Full C++ pipeline: Read VTK StructuredPoints and prepare mesh for Unreal Engine

#include <vtkSmartPointer.h>
#include <vtkStructuredPointsReader.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkTriangleFilter.h>
#include <vtkPointData.h>
#include <vtkFloatArray.h>
#include <iostream>

// Unreal-specific includes assumed
#include "ProceduralMeshComponent.h"

void ConvertVTKToUnrealMesh(const std::string& filePath, UProceduralMeshComponent* MeshComponent)
{
    vtkNew<vtkStructuredPointsReader> reader;
    reader->SetFileName(filePath.c_str());
    reader->Update();

    vtkStructuredPoints* input = reader->GetOutput();

    vtkNew<vtkDataSetSurfaceFilter> surfaceFilter;
    surfaceFilter->SetInputData(input);
    surfaceFilter->Update();

    vtkNew<vtkCleanPolyData> clean;
    clean->SetInputConnection(surfaceFilter->GetOutputPort());
    clean->Update();

    vtkNew<vtkTriangleFilter> triangleFilter;
    triangleFilter->SetInputConnection(clean->GetOutputPort());
    triangleFilter->Update();

    vtkNew<vtkPolyDataNormals> normalsFilter;
    normalsFilter->SetInputConnection(triangleFilter->GetOutputPort());
    normalsFilter->ComputePointNormalsOn();
    normalsFilter->Update();

    vtkPolyData* poly = normalsFilter->GetOutput();
    vtkPoints* points = poly->GetPoints();
    vtkDataArray* normals = poly->GetPointData()->GetNormals();

    if (!points || !points->GetData()) {
        std::cerr << "Invalid or missing vtkPoints data!" << std::endl;
        return;
    }

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    // Copy points
    for (vtkIdType i = 0; i < points->GetNumberOfPoints(); ++i) {
        double p[3];
        points->GetPoint(i, p);
        Vertices.Add(FVector(p[0], p[1], p[2]));

        if (normals) {
            double n[3];
            normals->GetTuple(i, n);
            Normals.Add(FVector(n[0], n[1], n[2]));
        } else {
            Normals.Add(FVector::UpVector);
        }

        UVs.Add(FVector2D(0.0f, 0.0f));  // Placeholder
        Colors.Add(FLinearColor::White);
        Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));  // Unreal will recalc tangents
    }

    // Extract triangles
    vtkCellArray* cells = poly->GetPolys();
    vtkIdType npts = 0;
    const vtkIdType* ptIds = nullptr;

    for (cells->InitTraversal(); cells->GetNextCell(npts, ptIds);) {
        if (npts == 3) {
            Triangles.Add(ptIds[0]);
            Triangles.Add(ptIds[1]);
            Triangles.Add(ptIds[2]);
        }
    }

    MeshComponent->CreateMeshSection_LinearColor(
        0,
        Vertices,
        Triangles,
        Normals,
        UVs,
        Colors,
        Tangents,
        true);
}
