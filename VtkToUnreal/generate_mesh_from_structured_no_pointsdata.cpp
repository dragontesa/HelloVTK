// Converts a structured points (.vtk) volume into a mesh by generating geometry
// from implicit data using VTK, and then prepares it for Unreal Engine rendering.

#include <vtkSmartPointer.h>
#include <vtkStructuredPointsReader.h>
#include <vtkImageData.h>
#include <vtkMarchingCubes.h>
#include <vtkTriangleFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkCleanPolyData.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <iostream>

// Unreal includes
#include "ProceduralMeshComponent.h"

void GenerateMeshFromVolume(const std::string& filePath, UProceduralMeshComponent* MeshComponent, double isoValue)
{
    vtkNew<vtkStructuredPointsReader> reader;
    reader->SetFileName(filePath.c_str());
    reader->Update();

    vtkImageData* imageData = reader->GetOutput();
    if (!imageData || !imageData->GetPointData()->GetScalars()) {
        std::cerr << "Input volume missing scalar data." << std::endl;
        return;
    }

    // Extract isosurface using Marching Cubes
    vtkNew<vtkMarchingCubes> mc;
    mc->SetInputData(imageData);
    mc->SetValue(0, isoValue);
    mc->Update();

    vtkNew<vtkTriangleFilter> triangleFilter;
    triangleFilter->SetInputConnection(mc->GetOutputPort());
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

    if (!points || !points->GetData()) {
        std::cerr << "Invalid point data." << std::endl;
        return;
    }

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
