#include <vtkActor.h>
#include <vtkCubeSource.h> // Using vtkCubeSource for a simple demonstrable mesh
#include <vtkNamedColors.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkNew.h> // For vtkNew smart pointers (VTK 8.x and later)
#include <vtkInteractorStyleTrackballCamera.h> // Base style for camera interaction
#include <vtkObjectFactory.h> // Needed for vtkStandardNewMacro

// Define a custom interactor style
// This style inherits from vtkInteractorStyleTrackballCamera to keep its
// default camera interaction (rotate, pan, zoom) and adds custom behavior.
class MyCustomInteractorStyle : public vtkInteractorStyleTrackballCamera {
public:
    // Standard VTK macro to create new instances of this class
    static MyCustomInteractorStyle* New();
    vtkTypeMacro(MyCustomInteractorStyle, vtkInteractorStyleTrackballCamera);

    // Override the OnKeyPress event handler
    virtual void OnKeyPress() override {
        // Get the key that was pressed
        std::string key = this->Interactor->GetKeySym();

        // Check for specific keys and perform custom actions
        if (key == "q") {
            std::cout << "DEBUG: 'Q' key pressed! Performing custom action (e.g., quitting, or toggling feature)." << std::endl;
            // You could add your custom logic here, like:
            // this->Interactor->GetRenderWindow()->Finalize();
            // exit(EXIT_SUCCESS);
        } else if (key == "c") {
            std::cout << "DEBUG: 'C' key pressed! Cube color changed!" << std::endl;
            // Example: Change the cube's color (requires access to the actor,
            // which you would typically pass to the style or get from the renderer)
            // For simplicity here, just printing.
        }

        // Call the superclass's OnKeyPress to maintain default key press behaviors
        // (e.g., 'r' for reset camera, 'f' for fly mode, etc.)
        vtkInteractorStyleTrackballCamera::OnKeyPress();
    }

    // You can override other event handlers like OnLeftButtonDown, OnMouseMove, etc.
    /*
    virtual void OnLeftButtonDown() override {
        std::cout << "DEBUG: Left mouse button pressed." << std::endl;
        // Perform custom action, then call superclass
        vtkInteractorStyleTrackballCamera::OnLeftButtonDown();
    }
    */
};
// Implement the New() method using VTK's macro
vtkStandardNewMacro(MyCustomInteractorStyle);


int main(int, char *[]) {
    // 1. Create a Cube Source
    vtkNew<vtkCubeSource> cubeSource;
    cubeSource->SetCenter(0.0, 0.0, 0.0);
    cubeSource->SetXLength(1.0);
    cubeSource->SetYLength(1.0);
    cubeSource->SetZLength(1.0);
    cubeSource->Update();

    // 2. Create a Mapper
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(cubeSource->GetOutputPort());

    // 3. Create an Actor
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);

    vtkNew<vtkNamedColors> colors;
    actor->GetProperty()->SetColor(colors->GetColor3d("Tomato").GetData());

    // 4. Create a Renderer
    vtkNew<vtkRenderer> renderer;
    renderer->AddActor(actor);
    renderer->SetBackground(colors->GetColor3d("SlateGrey").GetData());

    // 5. Create a Render Window
    vtkNew<vtkRenderWindow> renderWindow;
    renderWindow->AddRenderer(renderer);
    renderWindow->SetWindowName("VTK Interactive Mesh Viewer with Custom Style");
    renderWindow->SetSize(800, 600);

    // 6. Create a Render Window Interactor
    vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
    renderWindowInteractor->SetRenderWindow(renderWindow);

    // --- IMPORTANT: Set our custom interactor style ---
    vtkNew<MyCustomInteractorStyle> style;
    renderWindowInteractor->SetInteractorStyle(style);
    // ----------------------------------------------------

    // Initialize the interactor and start the event loop
    renderWindow->Render();
    renderWindowInteractor->Initialize();
    renderWindowInteractor->Start();

    return 0;
}
