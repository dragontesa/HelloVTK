/**
 * You are absolutely correct! My apologies. The vtkMemory.h header and vtkMemorySetFunctions were part of an older VTK design for custom allocators and have been removed/refactored in more recent VTK versions (like 9.x).

This is a very important distinction, and it means the approach I suggested (using vtkMemorySetFunctions) won't work directly with VTK 9.4.1.
VTK 9.x Custom Allocator Approach

In VTK 9.x, the way to provide custom memory management for arrays like vtkFloatArray is generally through a more explicit mechanism when you construct the arrays, or by providing custom FreeFunction callbacks.

Here's the updated strategy:

    For vtkDataArray (like vtkFloatArray): SetArray with a custom deleteMethod and void* pointer.
    Instead of letting InsertNextTuple allocate and reallocate, you pre-allocate the memory using Unreal's FMemory::Malloc and then tell vtkFloatArray to use that memory, without letting VTK free it. You'll be responsible for freeing it when the vtkFloatArray is no longer needed.
 */

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "HAL/MemoryBase.h" // For FMemory

// VTK Includes
#include "vtkNew.h"
#include "vtkFloatArray.h"
#include "vtkDataArray.h" // Needed for the free function typedef

// Define a custom free function that uses FMemory::Free
// This function signature must match vtkDataArray::FreeFunction
void CustomVtkFree(void* ptr)
{
    if (ptr)
    {
        FMemory::Free(ptr);
    }
}

class FMyVtkPluginModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        UE_LOG(LogTemp, Warning, TEXT("MyVTKPlugin: StartupModule"));

        // --- VTK Array Creation using Unreal's allocator ---

        // 1. Determine the size needed
        const int NumTuples = 8;
        const int NumComponents = 4;
        const int TotalElements = NumTuples * NumComponents;
        const size_t BufferSizeBytes = TotalElements * sizeof(float);

        // 2. Allocate the raw memory using Unreal's FMemory
        float* RawDataBuffer = (float*)FMemory::Malloc(BufferSizeBytes);
        if (!RawDataBuffer)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to allocate memory for VTK array!"));
            return;
        }

        // 3. Populate the raw buffer (copy your colorMap data)
        float colorMap[8][4] =
        {
            { 0.0, 0.0, 0.0, 1.0 },
            { 0.3, 0.0, 0.0, 1.0 },
            { 0.6, 0.0, 0.0, 1.0 },
            { 0.9, 0.0, 0.0, 1.0 },
            { 0.9, 0.3, 0.3, 1.0 },
            { 0.9, 0.6, 0.6, 1.0 },
            { 0.9, 0.9, 0.9, 1.0 },
            { 1.0, 1.0, 1.0, 1.0 }
        };
        FMemory::Memcpy(RawDataBuffer, colorMap, BufferSizeBytes);


        vtkNew<vtkFloatArray> array;
        // Set the raw array data and tell VTK to use our custom free function
        // The third argument (1) means VTK will manage (and free) this memory
        // but it will use the provided CustomVtkFree function to do so.
        // THIS IS THE CRITICAL CHANGE!
        array->SetArray(RawDataBuffer, TotalElements, CustomVtkFree);
        array->SetNumberOfComponents(NumComponents);
        // array->SetNumberOfTuples() is now implicitly handled by TotalElements and NumComponents

        int numComps = array->GetNumberOfComponents();
        int numTuples = array->GetNumberOfTuples();
        UE_LOG(LogTemp, Warning, TEXT("Test vtk array numComp=%d, numTuple=%d"), numComps, numTuples);

        // You can still access data like this:
        for (int i = 0; i < NumTuples; ++i)
        {
            float tuple[NumComponents];
            array->GetTuple(i, tuple);
            UE_LOG(LogTemp, Log, TEXT("Tuple %d: %.2f, %.2f, %.2f, %.2f"), i, tuple[0], tuple[1], tuple[2], tuple[3]);
        }

        // Note: When 'array' goes out of scope (or is explicitly deleted),
        // VTK will call CustomVtkFree(RawDataBuffer) to release the memory.
    }

    virtual void ShutdownModule() override
    {
        UE_LOG(LogTemp, Warning, TEXT("MyVTKPlugin: ShutdownModule"));
        // Any necessary cleanup of VTK objects, etc.
    }
};

IMPLEMENT_MODULE(FMyVtkPluginModule, MyVtkPlugin)




/**
 * Explanation of SetArray(void* data, vtkIdType size, FreeFunction method):

    data: A pointer to the raw memory buffer that VTK should use.
    size: The total number of elements (not bytes) in the array. For float and 4 components, 8 tuples: 8times4=32 elements.
    method: This is a vtkDataArray::FreeFunction pointer.
        If you pass nullptr or VTK_DATA_ARRAY_DELETE (which is 0), VTK assumes it owns the memory and will use its default allocation/free mechanism (which likely links to glibc's malloc). This is what was causing your problem.
        If you pass a valid function pointer (like CustomVtkFree), VTK will call your provided function when it needs to deallocate the memory. This forces VTK to use Unreal's FMemory::Free.

Important Considerations:

    Reallocation: This SetArray approach means you're providing a fixed-size buffer. If you later call InsertNextTuple and the array needs to grow beyond TotalElements, VTK might try to reallocate it. If it tries to realloc using its internal (default glibc) mechanism, you could still hit issues.
        Recommendation: If your array size is known, use SetArray with FMemory::Malloc and CustomVtkFree. If it needs to be dynamic, you'd have to manage the reallocation yourself (e.g., allocate a larger FMemory::Malloc'd buffer, copy data, and then call array->SetArray again with the new buffer). This gets more complex.
        For your colorMap example, the size is fixed, so this SetArray approach is perfect.

    vtkSmartPointer and vtkNew: These still work correctly. vtkNew manages the lifecycle of the vtkFloatArray object itself. The SetArray method tells the vtkFloatArray object how to manage its internal data buffer.

This updated approach directly addresses the mixed-allocator problem by ensuring the data buffer used by vtkFloatArray is managed by FMemory via your custom free function. This is the most likely solution for your 31 components / 1 tuple issue given libstdc++ matching and FMallocMimalloc being present.
 */