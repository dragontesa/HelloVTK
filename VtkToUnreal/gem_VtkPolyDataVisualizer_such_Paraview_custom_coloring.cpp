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

    // Scalar range for mapping values to the colormap [Min, Max]
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VTK Mesh")
    FVector2D ScalarRange = FVector2D(0.0f, 6.0f); // Adjusted default range based on your cell_scalars (0 to 5)

    // Name of the scalar array to use for coloring (e.g., "custom_table_scalars" or "cell_scalars")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VTK Mesh")
    FString ScalarArrayName = TEXT("cell_scalars"); // Default to cell_scalars for discrete coloring

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

    // Helper to parse the lookup table from the VTK file content string (now optional)
    TArray<FLinearColor> ParseLookupTable(const FString& VtkFileContent, const FString& TableName, int32 ExpectedEntries);

    // Function to map a scalar value to a color using a predefined colormap
    FLinearColor MapScalarToColor(double ScalarValue, const TArray<FLinearColor>& ColorMap, const FVector2D& CurrentScalarRange);
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


FLinearColor AVtkPolyDataVisualizer::MapScalarToColor(double ScalarValue, const TArray<FLinearColor>& ColorMap, const FVector2D& CurrentScalarRange)
{
    if (ColorMap.Num() == 0)
    {
        return FLinearColor::White; // Return white if no colormap is provided
    }

    // Normalize the scalar value to the [0, 1] range based on the CurrentScalarRange
    double NormalizedScalar = FMath::GetMappedRangeValueClamped(
        FVector2D(CurrentScalarRange.X, CurrentScalarRange.Y),
        FVector2D(0.0, 1.0),
        ScalarValue
    );

    // Map the normalized scalar to an index in the colormap
    // This interpolates between the colors in the colormap array
    float IndexF = NormalizedScalar * (ColorMap.Num() - 1);
    int32 Index1 = FMath::FloorToInt(IndexF);
    int32 Index2 = FMath::CeilToInt(IndexF);
    float Alpha = IndexF - Index1;

    // Clamp indices to ensure they are within the valid range
    Index1 = FMath::Clamp(Index1, 0, ColorMap.Num() - 1);
    Index2 = FMath::Clamp(Index2, 0, ColorMap.Num() - 1);

    // Interpolate between the two closest colors in the colormap
    return FMath::Lerp(ColorMap[Index1], ColorMap[Index2], Alpha);
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

    // --- Normals and Scalars (for per-face coloring) ---
    FString VtkFileContent;
    FFileHelper::LoadFileToString(VtkFileContent, *(FPaths::ProjectContentDir() / VtkFilePath.FilePath));
    // The ParseLookupTable is still here but we'll use a hardcoded color map for the discrete effect
    TArray<FLinearColor> ParsedLookupTable = ParseLookupTable(VtkFileContent, TEXT("my_table"), 8);

    // Define a discrete colormap for the stepped effect (e.g., 6 distinct colors for 6 faces)
    // These colors are chosen to roughly match the appearance in your Paraview_custom_coloring_cube.png
    TArray<FLinearColor> DiscreteColorMap;
    DiscreteColorMap.Add(FLinearColor(0.8f, 0.2f, 0.2f)); // Reddish (outermost)
    DiscreteColorMap.Add(FLinearColor(0.9f, 0.4f, 0.4f)); // Lighter Red
    DiscreteColorMap.Add(FLinearColor(0.95f, 0.6f, 0.6f)); // Even Lighter Red
    DiscreteColorMap.Add(FLinearColor(0.7f, 0.7f, 0.7f)); // Grayish (middle)
    DiscreteColorMap.Add(FLinearColor(0.5f, 0.5f, 0.5f)); // Darker Gray
    DiscreteColorMap.Add(FLinearColor(0.3f, 0.3f, 0.3f)); // Darkest Gray (innermost)

    // Get the scalar array based on the UPROPERTY ScalarArrayName
    vtkDataArray* vtkScalars = nullptr;
    if (ScalarArrayName == TEXT("cell_scalars"))
    {
        vtkScalars = InPolyData->GetCellData()->GetScalars("cell_scalars");
    }
    else if (ScalarArrayName == TEXT("custom_table_scalars"))
    {
        vtkScalars = InPolyData->GetPointData()->GetScalars("custom_table_scalars");
    }
    
    if (!vtkScalars)
    {
        UE_LOG(LogTemp, Error, TEXT("Scalar array '%s' not found in VTK PolyData. Defaulting to white faces."), *ScalarArrayName);
        // Fallback: If no scalar array found, use white for all faces
        vtkIdType NumCells = InPolyData->GetPolys()->GetNumberOfCells();
        OutColors.Init(FLinearColor::White, NumCells * 3); // Allocate for 3 vertices per triangle
    }

    // --- Cells (Triangles) and Edges ---
    vtkCellArray* vtkPolygons = InPolyData->GetPolys();
    if (!vtkPolygons)
    {
        UE_LOG(LogTemp, Warning, TEXT("No polygons found in VTK PolyData."));
        return;
    }

    vtkIdType NumCells = vtkPolygons->GetNumberOfCells();
    vtkIdList* CellPoints = vtkIdList::New();

    TSet<FIntPoint> UniqueEdges; // For edge rendering

    // Iterate through each cell (polygon) to create triangles for Unreal
    for (vtkIdType CellIdx = 0; CellIdx < NumCells; ++CellIdx)
    {
        vtkPolygons->GetCell(CellIdx, CellPoints);
        vtkIdType NumCellPoints = CellPoints->GetNumberOfIds();

        // Determine the color for this face
        FLinearColor FaceColor = FLinearColor::White; // Default color
        if (vtkScalars)
        {
            double scalarValue = 0.0;
            if (ScalarArrayName == TEXT("cell_scalars"))
            {
                scalarValue = vtkScalars->GetTuple1(CellIdx); // Get scalar for this cell
            }
            else // Assuming point data and using the first point's scalar for the face
            {
                if (NumCellPoints > 0)
                {
                    scalarValue = vtkScalars->GetTuple1(CellPoints->GetId(0));
                }
            }
            
            // Map the scalar value to a color using the discrete colormap
            FaceColor = MapScalarToColor(scalarValue, DiscreteColorMap, ScalarRange);
        }
        
        // Triangulate polygons and assign per-face color and normal
        if (NumCellPoints >= 3)
        {
            // Calculate face normal for flat shading
            FVector P0 = FVector(vtkPoints->GetPoint(CellPoints->GetId(0))[0], vtkPoints->GetPoint(CellPoints->GetId(0))[1], vtkPoints->GetPoint(CellPoints->GetId(0))[2]);
            FVector P1 = FVector(vtkPoints->GetPoint(CellPoints->GetId(1))[0], vtkPoints->GetPoint(CellPoints->GetId(1))[1], vtkPoints->GetPoint(CellPoints->GetId(1))[2]);
            FVector P2 = FVector(vtkPoints->GetPoint(CellPoints->GetId(2))[0], vtkPoints->GetPoint(CellPoints->GetId(2))[1], vtkPoints->GetPoint(CellPoints->GetId(2))[2]);
            FVector FaceNormal = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();

            // For each triangle in the polygon:
            // Add new vertices, normals, and colors.
            // This duplicates vertices that are shared between polygons, ensuring flat shading.
            // We use a new vertex index for each vertex of each triangle to ensure flat shading.
            for (vtkIdType j = 1; j < NumCellPoints - 1; ++j)
            {
                vtkIdType V0_Id = CellPoints->GetId(0);
                vtkIdType V1_Id = CellPoints->GetId(j);
                vtkIdType V2_Id = CellPoints->GetId(j + 1);

                // Add vertices for this triangle
                OutVertices.Add(FVector(vtkPoints->GetPoint(V0_Id)[0], vtkPoints->GetPoint(V0_Id)[1], vtkPoints->GetPoint(V0_Id)[2]));
                OutVertices.Add(FVector(vtkPoints->GetPoint(V1_Id)[0], vtkPoints->GetPoint(V1_Id)[1], vtkPoints->GetPoint(V1_Id)[2]));
                OutVertices.Add(FVector(vtkPoints->GetPoint(V2_Id)[0], vtkPoints->GetPoint(V2_Id)[1], vtkPoints->GetPoint(V2_Id)[2]));

                // Add triangle indices (relative to the new vertices just added)
                int32 CurrentBaseIndex = OutVertices.Num() - 3; // Index of the first vertex of this new triangle
                OutTriangles.Add(CurrentBaseIndex + 0);
                OutTriangles.Add(CurrentBaseIndex + 1);
                OutTriangles.Add(CurrentBaseIndex + 2);

                // Add normals for this triangle (flat normal)
                OutNormals.Add(FaceNormal);
                OutNormals.Add(FaceNormal);
                OutNormals.Add(FaceNormal);

                // Add colors for this triangle (per-face color)
                OutColors.Add(FaceColor);
                OutColors.Add(FaceColor);
                OutColors.Add(FaceColor);
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

// Helper function to parse a lookup table from the VTK file content string
// This function is now less critical as we are using a hardcoded colormap,
// but it remains for completeness if you wish to parse other LUTs.
TArray<FLinearColor> AVtkPolyDataVisualizer::ParseLookupTable(const FString& VtkFileContent, const FString& TableName, int32 ExpectedEntries)
{
    TArray<FLinearColor> LutColors;
    FString SearchString = FString::Printf(TEXT("LOOKUP_TABLE %s %d"), *TableName, ExpectedEntries);
    int32 StartIndex = VtkFileContent.Find(SearchString, ESearchCase::CaseSensitive);

    if (StartIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Error, TEXT("Lookup table '%s' definition not found in VTK file content. Using hardcoded colormap."), *TableName);
        return LutColors; // Return empty, will fall back to hardcoded or white
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
