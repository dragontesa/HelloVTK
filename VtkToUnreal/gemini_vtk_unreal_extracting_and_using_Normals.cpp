#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h" // For some utilities if needed

// VTK includes (ensure your build system links them)
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkIdList.h>
#include <vtkTriangleFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyDataReader.h> // Assuming you read from a .vtk file
#include <vtkSmartPointer.h>
#include <vtkNew.h> // If using vtkNew

// In your Unreal Actor or ActorComponent's .h file:
// UCLASS()
// class YOURPROJECT_API AMyVtkMeshActor : public AActor
// {
//     GENERATED BODY()
// public:
//     AMyVtkMeshActor();
//     virtual void BeginPlay() override;
//
//     UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
//     UProceduralMeshComponent* ProcMesh;
//
//     UFUNCTION(BlueprintCallable, Category = "VTK")
//     void LoadVtkMeshAndCreateProceduralMesh(FString FilePath);
// };

// In your Unreal Actor or ActorComponent's .cpp file:
// AMyVtkMeshActor::AMyVtkMeshActor()
// {
//     ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
//     RootComponent = ProcMesh;
// }
//
// void AMyVtkMeshActor::BeginPlay()
// {
//     Super::BeginPlay();
//     // Example: Load a mesh when the game starts
//     // LoadVtkMeshAndCreateProceduralMesh(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("my_data.vtk")));
// }


void AMyVtkMeshActor::LoadVtkMeshAndCreateProceduralMesh(FString FilePath)
{
    if (!ProcMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProcMesh is null!"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Attempting to load VTK mesh from: %s"), *FilePath);

    // --- VTK Reading and Normal Computation ---
    vtkNew<vtkPolyDataReader> reader;
    reader->SetFileName(TCHAR_TO_ANSI(*FilePath)); // Convert FString to const char*
    reader->Update();

    vtkSmartPointer<vtkPolyData> polyDataFromReader = reader->GetOutput();
    if (!polyDataFromReader || polyDataFromReader->GetNumberOfPoints() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("VTK: Failed to read PolyData or it's empty."));
        return;
    }

    // Step 1: Ensure Triangulation
    vtkNew<vtkTriangleFilter> triangleFilter;
    triangleFilter->SetInputData(polyDataFromReader);
    triangleFilter->Update();
    vtkSmartPointer<vtkPolyData> triangulatedPolyData = triangleFilter->GetOutput();

    // Step 2: Compute Normals
    vtkNew<vtkPolyDataNormals> normalsFilter;
    normalsFilter->SetInputData(triangulatedPolyData);
    normalsFilter->SetComputePointNormals(true);
    normalsFilter->SetAutoOrientNormals(true);
    normalsFilter->SetSplitting(false); // Adjust based on desired sharp/smooth edges
    normalsFilter->Update();
    vtkSmartPointer<vtkPolyData> polyDataWithNormals = normalsFilter->GetOutput();

    vtkPoints* vtkPoints = polyDataWithNormals->GetPoints();
    if (!vtkPoints) {
        UE_LOG(LogTemp, Error, TEXT("VTK: PolyData has no points after normal computation."));
        return;
    }

    vtkDataArray* vtkNormals = polyDataWithNormals->GetPointData()->GetNormals();
    if (!vtkNormals) {
        UE_LOG(LogTemp, Error, TEXT("VTK: No normals found after computation."));
        return;
    }

    // --- Data Extraction for Unreal ---
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;       // Placeholder (add real UVs if your VTK data has them)
    TArray<FLinearColor> VertexColors; // Placeholder (add real colors if your VTK data has them)
    TArray<FProcMeshTangent> Tangents; // Placeholder (compute tangents if needed for lighting)

    // Extract Vertices
    Vertices.Reserve(vtkPoints->GetNumberOfPoints());
    for (vtkIdType i = 0; i < vtkPoints->GetNumberOfPoints(); ++i) {
        double p[3];
        vtkPoints->GetPoint(i, p);
        // VTK uses Z-up, Unreal uses Z-up. X, Y, Z mapping is usually direct.
        // If your VTK data is Y-up or a different coordinate system, you'll need to re-map.
        Vertices.Add(FVector(p[0], p[1], p[2]));
    }
    UE_LOG(LogTemp, Log, TEXT("Extracted %d vertices."), Vertices.Num());

    // Extract Normals
    Normals.Reserve(vtkNormals->GetNumberOfTuples());
    for (vtkIdType i = 0; i < vtkNormals->GetNumberOfTuples(); ++i) {
        double n[3];
        vtkNormals->GetTuple(i, n);
        // Normalize the normal to ensure it's a unit vector
        // VTK normals are usually normalized, but it's good practice for rendering engines.
        FVector Normal = FVector(n[0], n[1], n[2]);
        Normal.Normalize(); // Ensure unit length
        Normals.Add(Normal);
    }
    UE_LOG(LogTemp, Log, TEXT("Extracted %d normals."), Normals.Num());


    // Extract Triangles (indices)
    // VTK's GetPolys() returns a vtkCellArray which contains cell types and point IDs.
    // For triangles, it will be 3 point IDs per cell.
    vtkCellArray* vtkPolys = polyDataWithNormals->GetPolys();
    if (!vtkPolys) {
        UE_LOG(LogTemp, Error, TEXT("VTK: PolyData has no polygons."));
        return;
    }

    vtkNew<vtkIdList> idList;
    vtkPolys->InitTraversal();
    while (vtkPolys->GetNextCell(idList)) {
        if (idList->GetNumberOfIds() == 3) { // Expecting triangles
            Triangles.Add(idList->GetId(0));
            Triangles.Add(idList->GetId(1));
            Triangles.Add(idList->GetId(2));
        } else {
            // This should not happen if vtkTriangleFilter was used correctly.
            UE_LOG(LogTemp, Warning, TEXT("VTK: Found non-triangular polygon after triangulation filter. Skipping."));
        }
    }
    UE_LOG(LogTemp, Log, TEXT("Extracted %d triangle indices."), Triangles.Num());

    // Initialize placeholders for other mesh data
    UV0.Init(FVector2D::ZeroVector, Vertices.Num());
    VertexColors.Init(FLinearColor::White, Vertices.Num());
    Tangents.Init(FProcMeshTangent(), Vertices.Num());

    // --- Create Mesh Section in ProceduralMeshComponent ---
    ProcMesh->CreateMeshSection(
        0,                 // SectionIndex
        Vertices,
        Triangles,
        Normals,
        UV0,
        VertexColors,
        Tangents,
        true               // CreateCollision (optional, generates simple collision)
    );

    // You can set the material here
    // static ConstructorHelpers::FObjectFinder<UMaterial> Material(TEXT("/Game/StarterContent/Materials/M_Basic_Wall"));
    // if (Material.Succeeded()) {
    //     ProcMesh->SetMaterial(0, Material.Object);
    // }
    UE_LOG(LogTemp, Log, TEXT("Procedural Mesh Section 0 created."));
}