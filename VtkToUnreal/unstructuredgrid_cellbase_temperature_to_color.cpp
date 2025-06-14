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
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkUnsignedCharArray.h>
#include <iostream>

#include "ProceduralMeshComponent.h"

extern FLinearColor TemperatureToColor(double scalar, double minVal, double maxVal);

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
    vtkDataArray* cellScalars = poly->GetCellData()->GetScalars();

    double minScalar = 0.0, maxScalar = 1.0;
    if (cellScalars) {
        cellScalars->GetRange(minScalar, maxScalar);
    }

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    vtkCellArray* cells = poly->GetPolys();
    vtkIdType npts;
    const vtkIdType* ptIds;
    int32 triangleIndex = 0;

    for (cells->InitTraversal(); cells->GetNextCell(npts, ptIds); ++triangleIndex) {
        if (npts == 3) {
            for (vtkIdType i = 0; i < 3; ++i) {
                double p[3];
                points->GetPoint(ptIds[i], p);
                Vertices.Add(FVector(p[0], p[1], p[2]));

                if (normals) {
                    double n[3];
                    normals->GetTuple(ptIds[i], n);
                    Normals.Add(FVector(n[0], n[1], n[2]));
                } else {
                    Normals.Add(FVector::UpVector);
                }

                UVs.Add(FVector2D(0.0f, 0.0f));

                FLinearColor color = FLinearColor::White;
                if (cellScalars) {
                    double scalar = cellScalars->GetTuple1(triangleIndex);
                    color = TemperatureToColor(scalar, minScalar, maxScalar);
                }
                Colors.Add(color);

                Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
            }

            int32 baseIdx = triangleIndex * 3;
            Triangles.Add(baseIdx);
            Triangles.Add(baseIdx + 1);
            Triangles.Add(baseIdx + 2);
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
