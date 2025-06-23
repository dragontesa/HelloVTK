#include <vtkActor.h>
#include <vtkCubeSource.h> // Using vtkCubeSource for a simple demonstrable mesh
#include <vtkNamedColors.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkNew.h> // For vtkNew smart pointers (VTK 8.x and later)

int main(int, char *[]) {
    // 1. Create a Cube Source
    // This will generate the geometric data for a cube.
    // In a real application, you might load a mesh from an OBJ or other file format
    // using readers like vtkOBJReader, vtkSTLReader, etc.
    vtkNew<vtkCubeSource> cubeSource;
    cubeSource->SetCenter(0.0, 0.0, 0.0); // Center the cube at the origin
    cubeSource->SetXLength(1.0);
    cubeSource->SetYLength(1.0);
    cubeSource->SetZLength(1.0);
    cubeSource->Update(); // Update the source to generate data

    // 2. Create a Mapper
    // The mapper takes the polygonal data from the source and maps it to graphics primitives.
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(cubeSource->GetOutputPort());

    // 3. Create an Actor
    // The actor represents the object (mesh) in the 3D scene.
    // It combines the geometric data from the mapper with properties like color, opacity, etc.
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);

    // Set the color of the cube
    vtkNew<vtkNamedColors> colors;
    actor->GetProperty()->SetColor(colors->GetColor3d("Tomato").GetData());

    // 4. Create a Renderer
    // The renderer manages the rendering process within a specific viewport.
    // It contains the actors (objects) to be rendered.
    vtkNew<vtkRenderer> renderer;
    renderer->AddActor(actor);
    renderer->SetBackground(colors->GetColor3d("SlateGrey").GetData()); // Set background color

    // 5. Create a Render Window
    // The render window is the actual window on the screen where the rendering appears.
    // It holds one or more renderers.
    vtkNew<vtkRenderWindow> renderWindow;
    renderWindow->AddRenderer(renderer);
    renderWindow->SetWindowName("VTK Interactive Mesh Viewer");
    renderWindow->SetSize(800, 600); // Set initial window size

    // 6. Create a Render Window Interactor
    // The interactor handles user events (mouse clicks, movements, keyboard presses).
    // It translates these events into camera manipulations (rotation, pan, zoom).
    vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
    renderWindowInteractor->SetRenderWindow(renderWindow);

    // Initialize the interactor and start the event loop
    // This will display the window and allow user interaction
    renderWindow->Render(); // Initial render to display the scene
    renderWindowInteractor->Initialize();
    renderWindowInteractor->Start(); // Start the event loop, waiting for user input

    return 0;
}
