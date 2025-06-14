// Converts 3D vtkPolyData (.vtk) directly into an Unreal Engine mesh
#include <vtkSmartPointer.h>
#include <vtkPolyDataReader.h>
#include <vtkTriangleFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <iostream>

#include "ProceduralMeshComponent.h"

void LoadPolyDataAndCreateMesh(const std::string& filePath, UProceduralMeshComponent* MeshComponent)
{
    vtkNew<vtkPolyDataReader> reader;
    reader->SetFileName(filePath.c_str());
    reader->Update();

    vtkPolyData* rawPoly = reader->GetOutput();
    if (!rawPoly || !rawPoly->GetPoints()) {
        std::cerr << "Invalid or empty vtkPolyData!" << std::endl;
        return;
    }

    // Ensure mesh is triangulated
    vtkNew<vtkTriangleFilter> triangleFilter;
    triangleFilter->SetInputData(rawPoly);
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
        Colors.Add(FLinearColor::White);
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
