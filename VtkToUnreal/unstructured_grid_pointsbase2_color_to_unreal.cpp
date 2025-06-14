// Converts vtkUnstructuredGrid (.vtk) to a triangle mesh for Unreal Engine
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGridReader.h>
#include <vtkGeometryFilter.h>
#include <vtkTriangleFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkUnsignedCharArray.h>
#include <iostream>

#include "ProceduralMeshComponent.h"

void LoadUnstructuredGridAndCreateMesh(const std::string& filePath, UProceduralMeshComponent* MeshComponent)
{
    vtkNew<vtkUnstructuredGridReader> reader;
    reader->SetFileName(filePath.c_str());
    reader->Update();

    vtkUnstructuredGrid* grid = reader->GetOutput();
    if (!grid || !grid->GetPoints()) {
        std::cerr << "Invalid or empty vtkUnstructuredGrid!" << std::endl;
        return;
    }

    // Convert unstructured grid to polydata using GeometryFilter
    vtkNew<vtkGeometryFilter> geometryFilter;
    geometryFilter->SetInputData(grid);
    geometryFilter->Update();

    vtkNew<vtkTriangleFilter> triangleFilter;
    triangleFilter->SetInputConnection(geometryFilter->GetOutputPort());
    triangleFilter->Update();

    vtkNew<vtkCleanPolyData> clean;
    clean->SetInputConnection(triangleFilter->GetOutputPort());
    clean->Update();

    vtkNew<vtkPolyDataNormals> normalsFilter;
    normalsFilter->SetInputConnection(clean->GetOutputPort());
    normalsFilter->ComputePointNormalsOn();
    normalsFilter->Update();

    vtkPolyData* poly = normalsFilter->GetOutput();
    vtkPoints* points = poly->GetPoints();
    vtkDataArray* normals = poly->GetPointData()->GetNormals();
    vtkUnsignedCharArray* colorArray = vtkUnsignedCharArray::SafeDownCast(poly->GetPointData()->GetScalars());

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

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

        UVs.Add(FVector2D(0.0f, 0.0f));

        if (colorArray && colorArray->GetNumberOfComponents() >= 3) {
            unsigned char rgba[4] = {255, 255, 255, 255};
            colorArray->GetTupleValue(i, rgba);
            float r = rgba[0] / 255.f;
            float g = rgba[1] / 255.f;
            float b = rgba[2] / 255.f;
            float a = (colorArray->GetNumberOfComponents() == 4) ? (rgba[3] / 255.f) : 1.0f;
            Colors.Add(FLinearColor(r, g, b, a));
        } else {
            Colors.Add(FLinearColor::White);
        }

        Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
    }

    vtkCellArray* cells = poly->GetPolys();
    vtkIdType npts;
    const vtkIdType* ptIds;

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
