
// FoamDataReader.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FoamDataReader.generated.h"

// Forward declaration for VTK PolyData to avoid VTK headers in the public API
class vtkPolyData;

/**
 * An Unreal Engine component to read OpenFOAM (.foam) data
 * and convert it into a VTK PolyData object.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MYUNREALPROJECT_API UFoamDataReaderComponent : public UActorComponent
{
    GENERATED_BODY()

public:	
    // Sets default values for this component's properties
    UFoamDataReaderComponent();

    // Function to read an OpenFOAM directory and return a vtkPolyData object
    // You must delete the returned vtkPolyData object with Delete() after you are done with it.
    UFUNCTION(BlueprintCallable, Category = "Foam Data")
    vtkPolyData* ReadFoamFile(const FString& FoamFilePath);
};

