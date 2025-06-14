// Converts 3D vtkPolyData (.vtk) directly into an Unreal Engine mesh, supporting cell- and point-based scalar color lookup tables
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
#include <vtkLookupTable.h>
#include <vtkScalarsToColors.h>
#include <vtkUnsignedCharArray.h>
#include <vtkFieldData.h>
#include <vtkObjectFactory.h>
#include <vtkGenericDataObjectReader.h>
#include <vtkFloatArray.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkPolyDataMapper.h>
#include <vtkStringArray.h>
#include <vtkAbstractArray.h>
#include <iostream>

#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "KismetProceduralMeshLibrary.h"

vtkSmartPointer<vtkLookupTable> ExtractLookupTableFromMapper(vtkPolyDataMapper* mapper)
{
    vtkScalarsToColors* scalarsToColors = mapper->GetLookupTable();
    if (!scalarsToColors)
    {
        std::cerr << "Mapper has no lookup table." << std::endl;
        return nullptr;
    }

    vtkLookupTable* lut = vtkLookupTable::SafeDownCast(scalarsToColors);
    if (!lut)
    {
        std::cerr << "Lookup table is not of type vtkLookupTable." << std::endl;
        return nullptr;
    }

    vtkSmartPointer<vtkLookupTable> copiedLUT = vtkSmartPointer<vtkLookupTable>::New();
    copiedLUT->DeepCopy(lut);
    return copiedLUT;
}

vtkUnsignedCharArray* MapScalarsWithCustomLUT(vtkPolyData* poly, const std::string& scalarName, const std::string& lookupTableName, bool useCellData)
{
    vtkSmartPointer<vtkDataArray> scalars = useCellData
        ? poly->GetCellData()->GetArray(scalarName.c_str())
        : poly->GetPointData()->GetArray(scalarName.c_str());

    if (!scalars) {
        std::cerr << "No scalar array found: " << scalarName << std::endl;
        return nullptr;
    }

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(poly);
    mapper->SetScalarVisibility(true);
    mapper->SetColorModeToMapScalars();
    mapper->SetScalarMode(useCellData ? VTK_SCALAR_MODE_USE_CELL_DATA : VTK_SCALAR_MODE_USE_POINT_DATA);
    mapper->SelectColorArray(scalarName.c_str());

    vtkSmartPointer<vtkLookupTable> lut = ExtractLookupTableFromMapper(mapper);
    if (!lut)
    {
        std::cerr << "Falling back to default hardcoded LUT." << std::endl;
        lut = vtkSmartPointer<vtkLookupTable>::New();
        lut->SetNumberOfTableValues(8);
        lut->Build();
        lut->SetTableValue(0, 0.0, 0.0, 0.0, 1.0);
        lut->SetTableValue(1, 0.3, 0.0, 0.0, 1.0);
        lut->SetTableValue(2, 0.6, 0.0, 0.0, 1.0);
        lut->SetTableValue(3, 0.9, 0.0, 0.0, 1.0);
        lut->SetTableValue(4, 0.9, 0.3, 0.3, 1.0);
        lut->SetTableValue(5, 0.9, 0.6, 0.6, 1.0);
        lut->SetTableValue(6, 0.9, 0.9, 0.9, 1.0);
        lut->SetTableValue(7, 1.0, 1.0, 1.0, 1.0);
    }

    mapper->SetLookupTable(lut);
    mapper->Update();

    return mapper->MapScalars(1.0);
}

void CreateUnrealMeshFromVTK(UWorld* World, vtkPolyData* PolyData, const std::string& ScalarName, bool bUseCellData)
{
    if (!PolyData || !World) return;

    vtkPoints* Points = PolyData->GetPoints();
    if (!Points) return;

    vtkUnsignedCharArray* Colors = MapScalarsWithCustomLUT(PolyData, ScalarName, "", bUseCellData);

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    vtkCellArray* Polys = PolyData->GetPolys();
    vtkIdType npts, *pts;

    for (Polys->InitTraversal(); Polys->GetNextCell(npts, pts);)
    {
        if (npts < 3) continue;
        for (vtkIdType i = 0; i < npts - 2; ++i)
        {
            int ids[3] = { static_cast<int>(pts[0]), static_cast<int>(pts[i + 1]), static_cast<int>(pts[i + 2]) };
            for (int id : ids)
            {
                double* p = Points->GetPoint(id);
                Vertices.Add(FVector(p[0], p[1], p[2]));
                Triangles.Add(Vertices.Num() - 1);

                if (Colors && id < Colors->GetNumberOfTuples())
                {
                    unsigned char* c = Colors->GetPointer(id * 4);
                    VertexColors.Add(FColor(c[0], c[1], c[2], c[3]));
                }
                else
                {
                    VertexColors.Add(FColor::White);
                }

                Normals.Add(FVector(0, 0, 1));
                UVs.Add(FVector2D(0, 0));
                Tangents.Add(FProcMeshTangent(1, 0, 0));
            }
        }
    }

    AActor* MeshActor = World->SpawnActor<AActor>();
    UProceduralMeshComponent* ProcMesh = NewObject<UProceduralMeshComponent>(MeshActor);
    ProcMesh->RegisterComponent();
    ProcMesh->AttachToComponent(MeshActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    ProcMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);
    MeshActor->SetRootComponent(ProcMesh);
}

// Example:
// CreateUnrealMeshFromVTK(GWorld, polyData, "custom_table_scalars", false);
