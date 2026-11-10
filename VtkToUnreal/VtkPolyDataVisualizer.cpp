// VtkPolyDataVisualizer.cpp
#include "VtkPolyDataVisualizer.h"
#include "Paths.h"
#include "Misc/FileHelper.h"

// VTK Includes
#include "vtkPolyDataReader.h"
#include "vtkTessellatorFilter.h"
#include "vtkPolyData.h"
#include "vtkPoints.h"
#include "vtkCellArray.h"
#include "vtkPointData.h"
#include "vtkCellData.h"
#include "vtkIdList.h"
#include "vtkDataSetSurfaceFilter.h"
#include "vtkUnstructuredGrid.h"
#include "vtkNew.h"
#include "vtkDataArray.h"

AVtkPolyDataVisualizer::AVtkPolyDataVisualizer()
{
    PrimaryActorTick.bCanEverTick = false; 

    SurfaceMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SurfaceMesh"));
    RootComponent = SurfaceMeshComponent;

    EdgeMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EdgeMesh"));
    EdgeMeshComponent->SetupAttachment(RootComponent); 

    // Default ParaView "Cool to Warm" spectrum
    CoolToWarmColorMapPoints.Add(FColorMapPoint(0.0f, FLinearColor(0.231373f, 0.298039f, 0.752941f, 1.0f)));
    CoolToWarmColorMapPoints.Add(FColorMapPoint(0.5f, FLinearColor(0.865003f, 0.865003f, 0.865003f, 1.0f)));
    CoolToWarmColorMapPoints.Add(FColorMapPoint(1.0f, FLinearColor(0.705882f, 0.0156863f, 0.14902f, 1.0f)));
}

void AVtkPolyDataVisualizer::BeginPlay()
{
    Super::BeginPlay();
    LoadAndVisualizeVtkData();
}

void AVtkPolyDataVisualizer::LoadAndVisualizeVtkData()
{
    if (VtkFilePath.FilePath.IsEmpty()) return;

    FString FullPath = FPaths::ProjectContentDir() / VtkFilePath.FilePath;
    if (!FPaths::FileExists(FullPath)) return;

    vtkNew<vtkPolyDataReader> reader;
    reader->SetFileName(TCHAR_TO_UTF8(*FullPath)); 
    reader->Update();

    vtkSmartPointer<vtkPolyData> polyData = reader->GetOutput();
    if (!polyData || polyData->GetNumberOfPoints() == 0) return;

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FLinearColor> Colors;
    TArray<FVector2D> UVs;
    TArray<int32> EdgeIndices;

    // Convert using our unified conversion pipeline
    ConvertVtkPolyDataToUnrealMesh(polyData, Vertices, Triangles, Normals, Colors, UVs, EdgeIndices);

    if (Vertices.Num() == 0 || Triangles.Num() == 0) return;

    // Check if the loaded VTK dataset is primarily composed of lines
    bool bHasLines = polyData->GetLines() && polyData->GetLines()->GetNumberOfCells() > 0;

    SurfaceMeshComponent->ClearAllMeshSections();
    
    // --- CRITICAL FIX 1: BOUNDS CULLING ---
    // Because the line mesh has 0 width on the CPU, Unreal calculates an extremely tight bounding box.
    // When the Vertex Shader expands the lines, they fall outside this box and Unreal hides them!
    // Increasing BoundsScale forces Unreal to keep the mesh visible even when WPO expands it.
    SurfaceMeshComponent->BoundsScale = 10.0f;

    // Create the section. If it's a line network, we disable physical collision calculation (!bHasLines) to optimize performance.
    SurfaceMeshComponent->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), !bHasLines);

    SurfaceMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    SurfaceMeshComponent->bUseComplexAsSimpleCollision = true;

    // --- CRITICAL FIX 2: DYNAMIC MATERIAL SELECTION ---
    if (bHasLines && LineMaterial)
    {
        // Apply the custom billboard expansion material to render the lines
        SurfaceMeshComponent->SetMaterial(0, LineMaterial);
        UE_LOG(LogTemp, Log, TEXT("VTK Visualizer: Applied LineMaterial to Section 0."));
    }
    else if (SurfaceMaterial)
    {
        // Fall back to standard surface material
        SurfaceMeshComponent->SetMaterial(0, SurfaceMaterial);
        UE_LOG(LogTemp, Log, TEXT("VTK Visualizer: Applied SurfaceMaterial to Section 0."));
    }
}

/**
 * Unified conversion function that detects whether the VTK dataset is:
 * 1. A 2D Surface (Polys > 0) -> Outputs standard flat-shaded triangles.
 * 2. A 1D Line network (Polys == 0, Lines > 0) -> Outputs zero-width billboardable ribbons.
 */
void AVtkPolyDataVisualizer::ConvertVtkPolyDataToUnrealMesh(
    vtkPolyData* InPolyData,
    TArray<FVector>& OutVertices,
    TArray<int32>& OutTriangles,
    TArray<FVector>& OutNormals,
    TArray<FLinearColor>& OutColors,
    TArray<FVector2D>& OutUVs,
    TArray<int32>& OutEdgeIndices)
{
    OutVertices.Empty();
    OutTriangles.Empty();
    OutNormals.Empty();
    OutColors.Empty();
    OutUVs.Empty();
    OutEdgeIndices.Empty();

    if (!InPolyData) return;

    vtkPoints* vtkPoints = InPolyData->GetPoints();
    if (!vtkPoints || vtkPoints->GetNumberOfPoints() == 0) return;

    // Determine scalar array for color mapping
    vtkDataArray* vtkScalars = nullptr;
    if (ScalarArrayName == TEXT("cell_scalars"))
    {
        vtkScalars = InPolyData->GetCellData()->GetScalars("cell_scalars");
    }
    else if (ScalarArrayName == TEXT("custom_table_scalars"))
    {
        vtkScalars = InPolyData->GetPointData()->GetScalars("custom_table_scalars");
    }
    
    // Fallback: If specified array is missing, look for the active scalar array
    if (!vtkScalars)
    {
        vtkScalars = InPolyData->GetPointData()->GetScalars();
    }

    vtkCellArray* vtkPolygons = InPolyData->GetPolys();
    vtkCellArray* vtkLines = InPolyData->GetLines();

    // ==========================================
    // CASE A: DATASET CONTAINS 2D SURFACE CELLS
    // ==========================================
    if (vtkPolygons && vtkPolygons->GetNumberOfCells() > 0)
    {
        vtkIdType NumCells = vtkPolygons->GetNumberOfCells();
        vtkNew<vtkIdList> CellPoints;

        for (vtkIdType CellIdx = 0; CellIdx < NumCells; ++CellIdx)
        {
            vtkPolygons->GetCell(CellIdx, CellPoints);
            vtkIdType NumCellPoints = CellPoints->GetNumberOfIds();
            
            if (NumCellPoints >= 3)
            {
                // Calculate face normal
                FVector P0 = FVector(vtkPoints->GetPoint(CellPoints->GetId(0))[0], vtkPoints->GetPoint(CellPoints->GetId(0))[1], vtkPoints->GetPoint(CellPoints->GetId(0))[2]);
                FVector P1 = FVector(vtkPoints->GetPoint(CellPoints->GetId(1))[0], vtkPoints->GetPoint(CellPoints->GetId(1))[1], vtkPoints->GetPoint(CellPoints->GetId(1))[2]);
                FVector P2 = FVector(vtkPoints->GetPoint(CellPoints->GetId(2))[0], vtkPoints->GetPoint(CellPoints->GetId(2))[1], vtkPoints->GetPoint(CellPoints->GetId(2))[2]);
                FVector FaceNormal = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();

                for (vtkIdType j = 1; j < NumCellPoints - 1; ++j)
                {
                    vtkIdType V0_Id = CellPoints->GetId(0);
                    vtkIdType V1_Id = CellPoints->GetId(j);
                    vtkIdType V2_Id = CellPoints->GetId(j + 1);

                    // Add scaled positions
                    OutVertices.Add(FVector(vtkPoints->GetPoint(V0_Id)[0], vtkPoints->GetPoint(V0_Id)[1], vtkPoints->GetPoint(V0_Id)[2]) * 1000.0f);
                    OutVertices.Add(FVector(vtkPoints->GetPoint(V1_Id)[0], vtkPoints->GetPoint(V1_Id)[1], vtkPoints->GetPoint(V1_Id)[2]) * 1000.0f);
                    OutVertices.Add(FVector(vtkPoints->GetPoint(V2_Id)[0], vtkPoints->GetPoint(V2_Id)[1], vtkPoints->GetPoint(V2_Id)[2]) * 1000.0f);

                    int32 BaseIndex = OutVertices.Num() - 3;
                    OutTriangles.Add(BaseIndex + 0);
                    OutTriangles.Add(BaseIndex + 1);
                    OutTriangles.Add(BaseIndex + 2);

                    OutNormals.Add(FaceNormal);
                    OutNormals.Add(FaceNormal);
                    OutNormals.Add(FaceNormal);
                    
                    // Simple default UV layout for 2D surface
                    OutUVs.Add(FVector2D(0.f, 0.f));
                    OutUVs.Add(FVector2D(1.f, 0.f));
                    OutUVs.Add(FVector2D(0.f, 1.f));

                    if (vtkScalars)
                    {
                        OutColors.Add(MapScalarToColor(vtkScalars->GetTuple1(V0_Id), CoolToWarmColorMapPoints, ScalarRange));
                        OutColors.Add(MapScalarToColor(vtkScalars->GetTuple1(V1_Id), CoolToWarmColorMapPoints, ScalarRange));
                        OutColors.Add(MapScalarToColor(vtkScalars->GetTuple1(V2_Id), CoolToWarmColorMapPoints, ScalarRange));
                    }
                    else
                    {
                        OutColors.Add(FLinearColor::White);
                        OutColors.Add(FLinearColor::White);
                        OutColors.Add(FLinearColor::White);
                    }
                }
            }
        }
    }
    // ==========================================
    // CASE B: DATASET IS A 1D LINE NETWORK ONLY (NO TRIS)
    // ==========================================
    else if (vtkLines && vtkLines->GetNumberOfCells() > 0)
    {
        vtkIdType NumLines = vtkLines->GetNumberOfCells();
        vtkNew<vtkIdList> LinePoints;

        for (vtkIdType LineIdx = 0; LineIdx < NumLines; ++LineIdx)
        {
            vtkLines->GetCell(LineIdx, LinePoints);
            vtkIdType NumLinePoints = LinePoints->GetNumberOfIds();

            for (int32 j = 0; j < NumLinePoints - 1; ++j)
            {
                vtkIdType V0_Id = LinePoints->GetId(j);
                vtkIdType V1_Id = LinePoints->GetId(j + 1);

                double coord0[3], coord1[3];
                vtkPoints->GetPoint(V0_Id, coord0);
                vtkPoints->GetPoint(V1_Id, coord1);

                // Convert points and scale to Unreal coordinates (1000x)
                FVector P0 = FVector(coord0[0], coord0[1], coord0[2]) * 1000.0f;
                FVector P1 = FVector(coord1[0], coord1[1], coord1[2]) * 1000.0f;

                // 1D line segment direction vector
                FVector SegmentDir = (P1 - P0).GetSafeNormal();

                int32 BaseIdx = OutVertices.Num();

                // Add 4 identical vertices to construct a zero-width quad ribbon
                OutVertices.Add(P0); // V0 (Left)
                OutVertices.Add(P0); // V1 (Right)
                OutVertices.Add(P1); // V2 (Left)
                OutVertices.Add(P1); // V3 (Right)

                // Store segment direction inside the Normal array
                OutNormals.Add(SegmentDir);
                OutNormals.Add(SegmentDir);
                OutNormals.Add(SegmentDir);
                OutNormals.Add(SegmentDir);

                // Store vertex expansion signs in UV coordinates
                // FIXED TYPO: FVectorz2D -> FVector2D
                OutUVs.Add(FVector2D(-1.0f, 0.0f)); // V0: Offset Left
                OutUVs.Add(FVector2D(1.0f, 0.0f));  // V1: Offset Right
                OutUVs.Add(FVector2D(-1.0f, 1.0f)); // V2: Offset Left
                OutUVs.Add(FVector2D(1.0f, 1.0f));  // V3: Offset Right

                // Generate triangles for the line quad
                OutTriangles.Add(BaseIdx + 0);
                OutTriangles.Add(BaseIdx + 2);
                OutTriangles.Add(BaseIdx + 1);

                OutTriangles.Add(BaseIdx + 1);
                OutTriangles.Add(BaseIdx + 2);
                OutTriangles.Add(BaseIdx + 3);

                // Populate colors using the VTK scalar data mapping
                if (vtkScalars)
                {
                    FLinearColor ColorV0 = MapScalarToColor(vtkScalars->GetTuple1(V0_Id), CoolToWarmColorMapPoints, ScalarRange);
                    FLinearColor ColorV1 = MapScalarToColor(vtkScalars->GetTuple1(V1_Id), CoolToWarmColorMapPoints, ScalarRange);

                    OutColors.Add(ColorV0); // V0
                    OutColors.Add(ColorV0); // V1
                    OutColors.Add(ColorV1); // V2
                    OutColors.Add(ColorV1); // V3
                }
                else
                {
                    OutColors.Add(FLinearColor::White);
                    OutColors.Add(FLinearColor::White);
                    OutColors.Add(FLinearColor::White);
                    OutColors.Add(FLinearColor::White);
                }
            }
        }
    }
}

FLinearColor AVtkPolyDataVisualizer::MapScalarToColor(double ScalarValue, const TArray<FColorMapPoint>& ColorMapPoints, const FVector2D& CurrentScalarRange)
{
    if (ColorMapPoints.Num() == 0) return FLinearColor::White;

    double NormalizedScalar = FMath::GetMappedRangeValueClamped(CurrentScalarRange, FVector2D(0.0, 1.0), ScalarValue);

    int32 Index1 = 0;
    int32 Index2 = ColorMapPoints.Num() - 1;

    for (int32 i = 0; i < ColorMapPoints.Num(); ++i)
    {
        if (NormalizedScalar <= ColorMapPoints[i].Scalar)
        {
            Index2 = i;
            break;
        }
        Index1 = i;
    }

    if (NormalizedScalar <= ColorMapPoints[0].Scalar) return ColorMapPoints[0].Color;
    if (NormalizedScalar >= ColorMapPoints.Last().Scalar) return ColorMapPoints.Last().Color;

    float Alpha = FMath::GetMappedRangeValueClamped(
        FVector2D(ColorMapPoints[Index1].Scalar, ColorMapPoints[Index2].Scalar),
        FVector2D(0.0, 1.0),
        NormalizedScalar
    );

    return FMath::Lerp(ColorMapPoints[Index1].Color, ColorMapPoints[Index2].Color, Alpha);
}