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
    normalsFilter->ComputePointNormalsOn(); // Enable both types for fallback
    normalsFilter->ComputeCellNormalsOn();
    normalsFilter->Update();

    vtkPolyData* poly = normalsFilter->GetOutput();
    vtkPoints* points = poly->GetPoints();
    vtkDataArray* cellNormals = poly->GetCellData()->GetNormals();
    vtkDataArray* pointNormals = poly->GetPointData()->GetNormals();

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    // Compute bounds for basic UV projection
    double bounds[6];
    poly->GetBounds(bounds);
    double dx = bounds[1] - bounds[0];
    double dy = bounds[3] - bounds[2];

    for (vtkIdType i = 0; i < points->GetNumberOfPoints(); ++i) {
        double p[3];
        points->GetPoint(i, p);
        Vertices.Add(FVector(p[0], p[1], p[2]));

        FVector normal = FVector::UpVector;
        if (pointNormals) {
            double n[3];
            pointNormals->GetTuple(i, n);
            normal = FVector(n[0], n[1], n[2]);
        }

        Normals.Add(normal);

        // Simple planar UV mapping (XY projection)
        float u = static_cast<float>((p[0] - bounds[0]) / dx);
        float v = static_cast<float>((p[1] - bounds[2]) / dy);
        UVs.Add(FVector2D(u, v));

        Colors.Add(FLinearColor::White);
        Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f)); // Placeholder
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

            if (!pointNormals && cellNormals) {
                double n[3];
                cellNormals->GetTuple(cellId, n);
                FVector normal(n[0], n[1], n[2]);
                Normals[ptIds[0]] = normal;
                Normals[ptIds[1]] = normal;
                Normals[ptIds[2]] = normal;
            }

            // Compute tangents from triangle edges
            const FVector& p0 = Vertices[ptIds[0]];
            const FVector& p1 = Vertices[ptIds[1]];
            const FVector& p2 = Vertices[ptIds[2]];

            const FVector2D& uv0 = UVs[ptIds[0]];
            const FVector2D& uv1 = UVs[ptIds[1]];
            const FVector2D& uv2 = UVs[ptIds[2]];

            FVector edge1 = p1 - p0;
            FVector edge2 = p2 - p0;
            FVector2D deltaUV1 = uv1 - uv0;
            FVector2D deltaUV2 = uv2 - uv0;

            float f = deltaUV1.X * deltaUV2.Y - deltaUV2.X * deltaUV1.Y;
            if (FMath::Abs(f) < KINDA_SMALL_NUMBER) f = 1.0f; else f = 1.0f / f;

            FVector tangent = f * (deltaUV2.Y * edge1 - deltaUV1.Y * edge2);
            tangent.Normalize();

            Tangents[ptIds[0]] = FProcMeshTangent(tangent, false);
            Tangents[ptIds[1]] = FProcMeshTangent(tangent, false);
            Tangents[ptIds[2]] = FProcMeshTangent(tangent, false);
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
