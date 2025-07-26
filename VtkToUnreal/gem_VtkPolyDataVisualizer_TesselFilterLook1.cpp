// VtkPolyDataVisualizer.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "VtkPolyDataVisualizer.generated.h"

// Forward declarations for VTK types to avoid full includes in header
class vtkPolyDataReader;
class vtkTessellatorFilter;
class vtkPolyData;
class vtkLookupTable;
class vtkDataArray;
class vtkIdList;
class vtkDataSetSurfaceFilter; // Added for the new filter
class vtkDataSet; // Added for generic dataset type

UCLASS()
class MYUNREALPROJECT_API AVtkPolyDataVisualizer : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    AVtkPolyDataVisualizer();

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

public:
    // Surface mesh component to display the tessellated VTK geometry
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VTK Mesh")
    UProceduralMeshComponent* SurfaceMeshComponent;

    // Edge mesh component to display the edges of the tessellated geometry
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VTK Mesh")
    UProceduralMeshComponent* EdgeMeshComponent; 

    // Material to apply to the surface mesh (should use vertex colors)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VTK Mesh")
    UMaterialInterface* SurfaceMaterial;

    // Material to apply to the edge mesh (should be a simple unlit color)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VTK Mesh")
    UMaterialInterface* EdgeMaterial;

    // Color for the rendered edges
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VTK Mesh")
    FLinearColor EdgeColor = FLinearColor::Black;

    // Line width for the edges (visual only, actual thickness depends on material/mesh generation)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VTK Mesh")
    float LineWidth = 2.0f; 

    // Path to the VTK file, relative to the project's Content directory
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VTK Mesh")
    FFilePath VtkFilePath;

    // Maximum number of subdivisions for the vtkTessellatorFilter
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VTK Mesh")
    int32 MaxSubdivisions = 3;

    // Function to load and visualize VTK data, callable from Blueprints
    UFUNCTION(BlueprintCallable, Category = "VTK Mesh")
    void LoadAndVisualizeVtkData();

private:
    // Helper function to convert VTK polydata to Unreal mesh data structures
    void ConvertVtkPolyDataToUnrealMesh(
        vtkPolyData* InPolyData,
        TArray<FVector>& OutVertices,
        TArray<int32>& OutTriangles,
        TArray<FVector>& OutNormals,
        TArray<FLinearColor>& OutColors,
        TArray<FVector2D>& OutUVs,
        TArray<int32>& OutEdgeIndices // Stores pairs of indices for edges
    );

    // Helper to parse the lookup table from the VTK file content string
    TArray<FLinearColor> ParseLookupTable(const FString& VtkFileContent, const FString& TableName, int32 ExpectedEntries);
};

```cpp
// VtkPolyDataVisualizer.cpp
#include "VtkPolyDataVisualizer.h"
#include "Paths.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h" // For async loading if needed, though not strictly used here

// VTK Includes (ensure these are correctly linked in your Build.cs)
#include "vtkPolyDataReader.h"
#include "vtkTessellatorFilter.h"
#include "vtkPolyData.h" // Now explicitly needed for SafeDownCast
#include "vtkPoints.h"
#include "vtkCellArray.h"
#include "vtkPointData.h"
#include "vtkFloatArray.h"
#include "vtkLookupTable.h" // Although we parse LUT manually, this might be needed for other VTK internal uses
#include "vtkIdList.h"
#include "vtkTriangle.h" // Used for triangulating polygons if needed
#include "vtkDataSetSurfaceFilter.h" // NEW: Required to convert UnstructuredGrid to PolyData
#include "vtkUnstructuredGrid.h" // NEW: To check if the output is an UnstructuredGrid
#include "vtkCellData.h" // For cell scalars if needed

// For custom memory allocator (from previous discussion)
#include "HAL/MemoryBase.h"
#include "vtkNew.h" // For vtkNew smart pointer
#include "vtkDataArray.h" // For VTK_DATA_ARRAY_USER_DEFINED and FreeFunction typedef

// Define a custom free function that uses FMemory::Free
// This function will be called by VTK when it needs to free memory
// for data arrays that were set using SetArray with VTK_DATA_ARRAY_USER_DEFINED.
void CustomVtkFree(void* ptr)
{
    if (ptr)
    {
        FMemory::Free(ptr);
    }
}

AVtkPolyDataVisualizer::AVtkPolyDataVisualizer()
{
    // No ticking needed for static visualization
    PrimaryActorTick.bCanEverTick = false; 

    // Create and set up the surface mesh component
    SurfaceMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SurfaceMesh"));
    RootComponent = SurfaceMeshComponent; // Set as the root component of the actor

    // Create and set up the edge mesh component, attached to the root
    EdgeMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EdgeMesh"));
    EdgeMeshComponent->SetupAttachment(RootComponent); 
}

void AVtkPolyDataVisualizer::BeginPlay()
{
    Super::BeginPlay();

    // Load and visualize the VTK data when the game starts
    LoadAndVisualizeVtkData();
}

void AVtkPolyDataVisualizer::LoadAndVisualizeVtkData()
{
    // Check if a VTK file path is provided
    if (VtkFilePath.FilePath.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("VTK File Path is empty! Please set it in the actor's details."));
        return;
    }

    // Construct the full path to the VTK file
    FString FullPath = FPaths::ProjectContentDir() / VtkFilePath.FilePath;
    if (!FPaths::FileExists(FullPath))
    {
    UE_LOG(LogTemp, Error, TEXT("VTK file not found at: %s"), *FullPath);
        return;
    }

    // --- VTK Pipeline Execution ---
    // Create a VTK PolyData reader
    vtkNew<vtkPolyDataReader> reader;
    // Set the file name (VTK expects UTF8 encoding for file paths)
    reader->SetFileName(TCHAR_TO_UTF8(*FullPath)); 
    // Execute the reader to load the data
    reader->Update();

    // Get the output polydata from the reader
    vtkSmartPointer<vtkPolyData> polyData = reader->GetOutput();
    if (!polyData || polyData->GetNumberOfPoints() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to read VTK PolyData or it's empty from file: %s"), *FullPath);
        return;
    }

    // Create a VTK Tessellator Filter
    vtkNew<vtkTessellatorFilter> tessel;
    // Set the input data for tessellation
    tessel->SetInputData(polyData);
    // Set the maximum number of subdivisions (e.g., 3 means 2^3 = 8 subdivisions per edge)
    tessel->SetMaximumNumberOfSubdivisions(MaxSubdivisions); 
    // Execute the tessellation filter
    tessel->Update();

    // Get the generic output from the tessellator
    vtkSmartPointer<vtkDataSet> tessellatorOutput = tessel->GetOutput();

    // Now, determine the actual type of the output and convert to PolyData if necessary
    vtkSmartPointer<vtkPolyData> processedData = nullptr;

    if (tessellatorOutput)
    {
        // Try to directly downcast to PolyData first
        processedData = vtkPolyData::SafeDownCast(tessellatorOutput);

        if (!processedData)
        {
            // If it's not PolyData, check if it's UnstructuredGrid
            vtkSmartPointer<vtkUnstructuredGrid> unstructuredGrid = vtkUnstructuredGrid::SafeDownCast(tessellatorOutput);
            if (unstructuredGrid)
            {
                UE_LOG(LogTemp, Warning, TEXT("Tessellator output is vtkUnstructuredGrid. Applying vtkDataSetSurfaceFilter."));
                // If it's UnstructuredGrid, extract its surface
                vtkNew<vtkDataSetSurfaceFilter> surfaceFilter;
                surfaceFilter->SetInputData(unstructuredGrid);
                surfaceFilter->Update();
                processedData = surfaceFilter->GetOutput();
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Tessellator output is neither vtkPolyData nor vtkUnstructuredGrid. ClassName: %s"), ANSI_TO_TCHAR(tessellatorOutput->GetClassName()));
            }
        }
    }

    if (!processedData || processedData->GetNumberOfPoints() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Final processed data is empty or not vtkPolyData after tessellation and surface extraction."));
        return;
    }

    // --- Extract Data from VTK and Convert to Unreal Format ---
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FLinearColor> Colors;
    TArray<FVector2D> UVs; // Not used by the VTK example, but kept for completeness
    TArray<int32> EdgeIndices; // Stores pairs of vertex indices for edges

    ConvertVtkPolyDataToUnrealMesh(processedData, Vertices, Triangles, Normals, Colors, UVs, EdgeIndices);

    if (Vertices.Num() == 0 || Triangles.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("No valid mesh data extracted from VTK PolyData."));
        return;
    }

    // --- Create Surface Mesh Component ---
    // Clear any existing mesh sections
    SurfaceMeshComponent->ClearAllMeshSections();
    // Create a new mesh section with the extracted data
    SurfaceMeshComponent->CreateMeshSection(
        0,              // Section Index (can have multiple sections)
        Vertices,       // Vertex positions
        Triangles,      // Triangle indices
        Normals,        // Vertex normals (can be empty, PMC generates if needed)
        UVs,            // UV coordinates (empty if not provided)
        Colors,         // Vertex colors
        TArray<FProcMeshTangent>(), // Tangents (optional, empty here)
        false           // Do not create collision for this visual mesh
    );

    // Apply the surface material if set
    if (SurfaceMaterial)
    {
        SurfaceMeshComponent->SetMaterial(0, SurfaceMaterial);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SurfaceMaterial not set on AVtkPolyDataVisualizer. Default material will be used for surface."));
    }

    // --- Create Edge Mesh Component ---
    EdgeMeshComponent->ClearAllMeshSections();
    if (EdgeIndices.Num() > 0)
    {
        // To render lines in Unreal's ProceduralMeshComponent, we typically create
        // thin quads for each line segment. This is more robust than relying on
        // material tricks for line drawing.

        TArray<FVector> EdgeLineVertices;
        TArray<int32> EdgeLineIndices;
        TArray<FVector> EdgeLineNormals; // Not strictly needed for lines
        TArray<FLinearColor> EdgeLineColors;

        // Determine a small thickness for the lines based on LineWidth property
        // This is a rough conversion, adjust as needed for visual appearance.
        float ActualEdgeThickness = LineWidth * 0.005f; 
        if (ActualEdgeThickness < KINDA_SMALL_NUMBER) ActualEdgeThickness = 0.01f; // Minimum thickness

        for (int32 i = 0; i < EdgeIndices.Num(); i += 2)
        {
            FVector StartVert = Vertices[EdgeIndices[i]];
            FVector EndVert = Vertices[EdgeIndices[i+1]];

            FVector LineDir = (EndVert - StartVert).GetSafeNormal();
            // Find a perpendicular vector for offsetting the line to create a quad
            FVector Perpendicular = FVector::CrossProduct(LineDir, FVector::UpVector).GetSafeNormal();
            if (Perpendicular.IsNearlyZero()) // If line is vertical, cross with RightVector
            {
                Perpendicular = FVector::CrossProduct(LineDir, FVector::RightVector).GetSafeNormal();
            }

            FVector Offset = Perpendicular * ActualEdgeThickness * 0.5f;

            // Add 4 vertices for the quad representing the line segment
            int32 BaseIndex = EdgeLineVertices.Num();
            EdgeLineVertices.Add(StartVert - Offset); // Bottom-left
            EdgeLineVertices.Add(StartVert + Offset); // Top-left
            EdgeLineVertices.Add(EndVert + Offset);   // Top-right
            EdgeLineVertices.Add(EndVert - Offset);   // Bottom-right

            // Add 2 triangles to form the quad
            EdgeLineIndices.Add(BaseIndex + 0); EdgeLineIndices.Add(BaseIndex + 1); EdgeLineIndices.Add(BaseIndex + 2);
            EdgeLineIndices.Add(BaseIndex + 0); EdgeLineIndices.Add(BaseIndex + 2); EdgeLineIndices.Add(BaseIndex + 3);

            // Add normals (flat for the quad) and colors
            FVector QuadNormal = FVector::CrossProduct(
                EdgeLineVertices[BaseIndex+1] - EdgeLineVertices[BaseIndex], 
                EdgeLineVertices[BaseIndex+3] - EdgeLineVertices[BaseIndex]
            ).GetSafeNormal();

            EdgeLineNormals.Add(QuadNormal);
            EdgeLineNormals.Add(QuadNormal);
            EdgeLineNormals.Add(QuadNormal);
            EdgeLineNormals.Add(QuadNormal);

            EdgeLineColors.Add(EdgeColor);
            EdgeLineColors.Add(EdgeColor);
            EdgeLineColors.Add(EdgeColor);
            EdgeLineColors.Add(EdgeColor);
        }

        if (EdgeLineVertices.Num() > 0)
        {
            EdgeMeshComponent->CreateMeshSection(
                0,
                EdgeLineVertices,
                EdgeLineIndices,
                EdgeLineNormals,
                TArray<FVector2D>(), // No UVs for lines
                EdgeLineColors,
                TArray<FProcMeshTangent>(),
                false
            );

            if (EdgeMaterial)
            {
                EdgeMeshComponent->SetMaterial(0, EdgeMaterial);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("EdgeMaterial not set on AVtkPolyDataVisualizer. Default material will be used for edges."));
            }
        }
    }
}


void AVtkPolyDataVisualizer::ConvertVtkPolyDataToUnrealMesh(
    vtkPolyData* InPolyData,
    TArray<FVector>& OutVertices,
    TArray<int32>& OutTriangles,
    TArray<FVector>& OutNormals,
    TArray<FLinearColor>& OutColors,
    TArray<FVector2D>& OutUVs, // Not used in this specific example
    TArray<int32>& OutEdgeIndices)
{
    // Clear output arrays before filling
    OutVertices.Empty();
    OutTriangles.Empty();
    OutNormals.Empty();
    OutColors.Empty();
    OutUVs.Empty();
    OutEdgeIndices.Empty();

    // --- Points (Vertices) ---
    vtkPoints* vtkPoints = InPolyData->GetPoints();
    if (!vtkPoints)
    {
        UE_LOG(LogTemp, Error, TEXT("VTK PolyData has no points. Cannot convert to Unreal mesh."));
        return;
    }

    OutVertices.Reserve(vtkPoints->GetNumberOfPoints());
    // Iterate through VTK points and convert to Unreal FVector
    for (vtkIdType i = 0; i < vtkPoints->GetNumberOfPoints(); ++i)
    {
        double p[3];
        vtkPoints->GetPoint(i, p);
        // Assuming VTK's Z-up matches Unreal's Z-up.
        // If coordinate system differences occur, adjust here (e.g., FVector(p[0], p[2], p[1]))
        OutVertices.Add(FVector(p[0], p[1], p[2]));
    }

    // --- Normals ---
    // Get point normals from VTK's processed data (tessellator filter should generate them)
    vtkDataArray* vtkNormals = InPolyData->GetPointData()->GetNormals();
    if (vtkNormals)
    {
        OutNormals.Reserve(vtkNormals->GetNumberOfTuples());
        for (vtkIdType i = 0; i < vtkNormals->GetNumberOfTuples(); ++i)
        {
            double n[3];
            vtkNormals->GetTuple(i, n);
            OutNormals.Add(FVector(n[0], n[1], n[2]));
        }
    }
    else
    {
        // If VTK doesn't provide normals, ProceduralMeshComponent can generate them automatically.
        UE_LOG(LogTemp, Warning, TEXT("No normals found in VTK PolyData. ProceduralMeshComponent will generate them if needed."));
    }

    // --- Scalars and Colors (using the 'custom_table_scalars' and 'my_table' lookup table) ---
    FString VtkFileContent;
    // Load the VTK file content as a string to parse the embedded lookup table
    FFileHelper::LoadFileToString(VtkFileContent, *(FPaths::ProjectContentDir() / VtkFilePath.FilePath));
    TArray<FLinearColor> ParsedLookupTable = ParseLookupTable(VtkFileContent, TEXT("my_table"), 8);

    // Get the scalar array by name
    vtkDataArray* vtkScalars = InPolyData->GetPointData()->GetScalars("custom_table_scalars");
    if (vtkScalars && ParsedLookupTable.Num() > 0)
    {
        OutColors.Reserve(vtkScalars->GetNumberOfTuples());
        // Define the scalar range for mapping to the lookup table.
        // This is assumed from the provided VTK file's LUT definition (0.0 to 1.0).
        double ScalarRange[2] = { 0.0, 1.0 }; 

        for (vtkIdType i = 0; i < vtkScalars->GetNumberOfTuples(); ++i)
        {
            double scalarValue = vtkScalars->GetTuple1(i); // Get single scalar value

            // Normalize scalar value to the [0, 1] range based on the defined ScalarRange
            double normalizedScalar = FMath::GetMappedRangeValueClamped(
                FVector2D(ScalarRange[0], ScalarRange[1]),
                FVector2D(0.0, 1.0),
                scalarValue
            );

            // Map the normalized scalar to an index in the parsed lookup table
            int32 LutIndex = FMath::RoundToInt(normalizedScalar * (ParsedLookupTable.Num() - 1));
            LutIndex = FMath::Clamp(LutIndex, 0, ParsedLookupTable.Num() - 1);

            OutColors.Add(ParsedLookupTable[LutIndex]);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Scalar data 'custom_table_scalars' or 'my_table' lookup table not found. Defaulting to white vertex colors."));
        // If scalars or LUT not found, default to white for all vertices
        OutColors.Init(FLinearColor::White, OutVertices.Num());
    }

    // --- Cells (Triangles) and Edges ---
    vtkCellArray* vtkPolygons = InPolyData->GetPolys();
    if (vtkPolygons)
    {
        vtkIdType NumCells = vtkPolygons->GetNumberOfCells();
        vtkIdList* CellPoints = vtkIdList::New();

        // Use a set to store unique edges to avoid duplicates when generating line geometry
        TSet<FIntPoint> UniqueEdges;

        for (vtkIdType i = 0; i < NumCells; ++i)
        {
            // Get the point IDs for the current cell
            vtkPolygons->GetCell(i, CellPoints);
            vtkIdType NumCellPoints = CellPoints->GetNumberOfIds();

            // Triangulate polygons for Unreal's ProceduralMeshComponent
            // The input VTK file uses 4-vertex polygons (quads).
            // For a quad (V0, V1, V2, V3), we create two triangles (V0,V1,V2) and (V0,V2,V3).
            // For general N-gons, a fan triangulation from the first vertex is used (V0, V1, V2), (V0, V2, V3), ...
            if (NumCellPoints >= 3)
            {
                vtkIdType V0 = CellPoints->GetId(0);
                for (vtkIdType j = 1; j < NumCellPoints - 1; ++j)
                {
                    vtkIdType V1 = CellPoints->GetId(j);
                    vtkIdType V2 = CellPoints->GetId(j + 1);

                    // Add triangle indices (referencing the original vertices)
                    OutTriangles.Add(V0);
                    OutTriangles.Add(V1);
                    OutTriangles.Add(V2);
                }

                // Collect edges from the original polygon for the edge mesh component
                for (vtkIdType j = 0; j < NumCellPoints; ++j)
                {
                    vtkIdType VtxA = CellPoints->GetId(j);
                    vtkIdType VtxB = CellPoints->GetId((j + 1) % NumCellPoints);
                    
                    // Ensure consistent order (min, max) for unique edge detection in the set
                    if (VtxA > VtxB) Swap(VtxA, VtxB);
                    UniqueEdges.Add(FIntPoint(VtxA, VtxB));
                }
            }
        }
        // Clean up VTK IdList
        CellPoints->Delete();

        // Convert the set of unique edges into a linear array of indices for the edge mesh
        for (const FIntPoint& Edge : UniqueEdges)
        {
            OutEdgeIndices.Add(Edge.X);
            OutEdgeIndices.Add(Edge.Y);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No polygons found in VTK PolyData."));
    }
}

// Helper function to parse a lookup table from the VTK file content string
TArray<FLinearColor> AVtkPolyDataVisualizer::ParseLookupTable(const FString& VtkFileContent, const FString& TableName, int32 ExpectedEntries)
{
    TArray<FLinearColor> LutColors;
    FString SearchString = FString::Printf(TEXT("LOOKUP_TABLE %s %d"), *TableName, ExpectedEntries);
    int32 StartIndex = VtkFileContent.Find(SearchString, ESearchCase::CaseSensitive);

    if (StartIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Error, TEXT("Lookup table '%s' definition not found in VTK file content."), *TableName);
        return LutColors;
    }

    // Find the end of the line where the lookup table definition is (to start parsing colors from next line)
    int32 EndOfDefLine = VtkFileContent.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIndex);
    if (EndOfDefLine == INDEX_NONE) EndOfDefLine = VtkFileContent.Len(); // Handle case where it's the last line

    // Start parsing from the character after the definition line
    int32 CurrentPos = EndOfDefLine + 1;

    for (int32 i = 0; i < ExpectedEntries; ++i)
    {
        // Find the end of the current color entry line
        int32 LineEnd = VtkFileContent.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, CurrentPos);
        if (LineEnd == INDEX_NONE) LineEnd = VtkFileContent.Len(); // Handle last line

        // Extract the line, trim whitespace
        FString Line = VtkFileContent.Mid(CurrentPos, LineEnd - CurrentPos).TrimStartAndEnd();
        CurrentPos = LineEnd + 1; // Move to the start of the next line

        // Parse color components (R G B A) separated by spaces
        TArray<FString> ColorComponents;
        Line.ParseIntoArray(ColorComponents, TEXT(" "), true);

        if (ColorComponents.Num() >= 4)
        {
            // Convert string components to float and add to LUT
            float R = FCString::Atof(*ColorComponents[0]);
            float G = FCString::Atof(*ColorComponents[1]);
            float B = FCString::Atof(*ColorComponents[2]);
            float A = FCString::Atof(*ColorComponents[3]);
            LutColors.Add(FLinearColor(R, G, B, A));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to parse color entry %d for lookup table '%s': '%s'. Expected 4 components."), i, *TableName, *Line);
            LutColors.Empty(); // Invalidate LUT if parsing fails
            break;
        }
    }

    return LutColors;
}
