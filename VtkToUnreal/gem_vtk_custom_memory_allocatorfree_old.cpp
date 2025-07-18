/**
 *  VTK 7 이하의 경우 Custom Allocator/Free
 */
// In your Plugin's .cpp file (e.g., MyVtkPlugin.cpp)
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "HAL/MemoryBase.h" // For FMemory
#include "vtkNew.h" // For vtkNew if needed, but not for the allocator
#include "vtkObject.h" // Includes vtkMemory.h usually
#include "vtkType.h" // For vtkIdType

// Forward declarations for VTK's custom memory functions
extern "C" {
    void* VtkMemoryAllocate(size_t size);
    void* VtkMemoryReallocate(void* ptr, size_t size);
    void VtkMemoryFree(void* ptr);
}

// Implement the wrappers to FMemory
void* VtkMemoryAllocate(size_t size)
{
    return FMemory::Malloc(size);
}

void* VtkMemoryReallocate(void* ptr, size_t size)
{
    return FMemory::Realloc(ptr, size);
}

void VtkMemoryFree(void* ptr)
{
    FMemory::Free(ptr);
}

class FMyVtkPluginModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        UE_LOG(LogTemp, Warning, TEXT("MyVTKPlugin: StartupModule"));

        // Set VTK's global memory functions to use Unreal's FMemory
        // This MUST be done before any VTK object allocates memory
        vtkMemorySetFunctions(VtkMemoryAllocate, VtkMemoryReallocate, VtkMemoryFree);

        // ... now you can create your VTK objects safely ...
        // Example of your original code:
        vtkNew<vtkFloatArray> array;
        array->SetNumberOfComponents(4);
        // No need to SetNumberOfTuples(8)
        for (int i=0;i<8;i++) {
            float colorMap[4] = { /* your color values */ }; // Re-declare inside loop if using this pattern
            // Or access from your global colorMap like before
            array->InsertNextTuple(colorMap[i]);
        }
        int numComps = array->GetNumberOfComponents();
        int numTuples = array->GetNumberOfTuples();
        UE_LOG(LogTemp, Warning, TEXT("Test vtk array numComp=%d, numTuple=%d"), numComps, numTuples);
        // ... rest of your VTK code ...
    }

    virtual void ShutdownModule() override
    {
        UE_LOG(LogTemp, Warning, TEXT("MyVTKPlugin: ShutdownModule"));
        // Cleanup VTK objects if necessary
        // ...
    }
};

IMPLEMENT_MODULE(FMyVtkPluginModule, MyVtkPlugin)




