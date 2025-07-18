// In your .h file or a common header:
#include "CoreMinimal.h" // For UE_LOG
#include "vtkOutputWindow.h"
#include "vtkObjectFactory.h" // Needed for vtkStandardNewMacro

// Define a custom VTK error handler class
class MyVtkErrorHandler : public vtkOutputWindow
{
public:
    static MyVtkErrorHandler* New();
    vtkTypeMacro(MyVtkErrorHandler, vtkOutputWindow);

    void DisplayText(const char* text) override
    {
        // Redirect VTK messages to Unreal's logging system
        // Convert char* to TCHAR* for UE_LOG
        UE_LOG(LogTemp, Error, TEXT("VTK Error/Warning: %s"), ANSI_TO_TCHAR(text));
    }
};
vtkStandardNewMacro(MyVtkErrorHandler);

// In your Unreal Module's StartupModule() or where you initialize VTK:
// (e.g., in YourProjectName.cpp or a custom UObject's constructor/BeginPlay)
void FYourModuleName::StartupModule()
{
    // ... other module init ...

    // Set custom VTK error handler
    vtkNew<MyVtkErrorHandler> errorHandler;
    vtkOutputWindow::SetInstance(errorHandler);

    // Optionally, disable VTK's default console output if it's redundant
    // vtkOutputWindow::GetInstance()->PromptUserOff();

    // ... other module init ...
}