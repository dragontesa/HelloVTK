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
#include <vtkCellData.h>
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
    normalsFilter->ComputePointNormalsOff();
    normalsFilter->ComputeCellNormalsOn();
    normalsFilter->Update();

    vtkPolyData* poly = normalsFilter->GetOutput();
    vtkPoints* points = poly->GetPoints();
    vtkDataArray* cellNormals = poly->GetCellData()->GetNormals();

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

        // Placeholder, actual normals added per-triangle below
        Normals.Add(FVector::UpVector);
        UVs.Add(FVector2D(0.0f, 0.0f));
        Colors.Add(FLinearColor::White);
        Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
    }

    vtkCellArray* cells = poly->GetPolys();
    vtkIdType npts;
    const vtkIdType* ptIds;
    vtkIdType cellId = 0;

    for (cells->InitTraversal(); cells->GetNextCell(npts, ptIds); ++cellId) {
        if (npts == 3) {
            Triangles.Add(ptIds[0]);
            Triangles.Add(ptIds[1]);
            Triangles.Add(ptIds[2]);

            if (cellNormals) {
                double n[3];
                cellNormals->GetTuple(cellId, n);
                FVector normal(n[0], n[1], n[2]);
                Normals[ptIds[0]] = normal;
                Normals[ptIds[1]] = normal;
                Normals[ptIds[2]] = normal;
            }
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
