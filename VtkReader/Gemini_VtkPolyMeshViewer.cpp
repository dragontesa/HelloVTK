#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkNew.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkObjectFactory.h>
#include <vtkPolyDataReader.h> // For reading .vtk files
#include <vtkPolyData.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkFloatArray.h>
#include <vtkIntArray.h> // For cell_scalars and cellIds
#include <vtkLookupTable.h>
#include <vtkNamedColors.h>
#include <vtkFieldData.h> // For accessing FieldData
#include <vtkDataArray.h> // Base class for data arrays

#include <fstream> // For creating the dummy VTK file
#include <iostream>
#include <string>
#include <vector>
#include <algorithm> // For std::min/max_element

// --- Custom Interactor Style ---
class MyCustomInteractorStyle : public vtkInteractorStyleTrackballCamera {
public:
    static MyCustomInteractorStyle* New();
    vtkTypeMacro(MyCustomInteractorStyle, vtkInteractorStyleTrackballCamera);

    virtual void OnKeyPress() override {
        std::string key = this->Interactor->GetKeySym();
        if (key == "q") {
            std::cout << "DEBUG: 'Q' key pressed! Performing custom action (e.g., quitting, or toggling feature)." << std::endl;
        } else if (key == "c") {
            std::cout << "DEBUG: 'C' key pressed! Custom action for 'c' key." << std::endl;
        }
        vtkInteractorStyleTrackballCamera::OnKeyPress();
    }
};
vtkStandardNewMacro(MyCustomInteractorStyle);


// --- Function to create the dummy .vtk file exactly as specified by the user ---
void CreateDummyVtkFile(const std::string& filename) {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        std::cerr << "Error: Could not create dummy VTK file: " << filename << std::endl;
        return;
    }

    // Header - VTK DataFile Version 2.0
    ofs << "# vtk DataFile Version 2.0\n";
    ofs << "Cube example\n";
    ofs << "ASCII\n";
    ofs << "DATASET POLYDATA\n";

    // POINTS (8 points for a cube)
    ofs << "POINTS 8 float\n";
    ofs << "0.0 0.0 0.0\n";
    ofs << "1.0 0.0 0.0\n";
    ofs << "1.0 1.0 0.0\n";
    ofs << "0.0 1.0 0.0\n";
    ofs << "0.0 0.0 1.0\n";
    ofs << "1.0 0.0 1.0\n";
    ofs << "1.0 1.0 1.0\n";
    ofs << "0.0 1.0 1.0\n";

    // POLYGONS (6 faces/quads for a cube - VTK 2.0 specific syntax)
    ofs << "POLYGONS 6 30\n"; // 6 cells, each is a quad (4 points + 1 cell type = 5 numbers) * 6 cells = 30 numbers
    ofs << "4 0 1 2 3\n"; // Front face
    ofs << "4 4 5 6 7\n"; // Back face
    ofs << "4 0 1 5 4\n"; // Bottom face
    ofs << "4 2 3 7 6\n"; // Top face
    ofs << "4 0 4 7 3\n"; // Left face
    ofs << "4 1 2 6 5\n"; // Right face

    // CELL_DATA
    ofs << "CELL_DATA 6\n";

    // SCALARS cell_scalars
    ofs << "SCALARS cell_scalars int 1\n";
    ofs << "LOOKUP_TABLE default\n";
    ofs << "0\n";
    ofs << "1\n";
    ofs << "2\n";
    ofs << "3\n";
    ofs << "4\n";
    ofs << "5\n";

    // NORMALS cell_normals
    ofs << "NORMALS cell_normals float\n";
    ofs << "0 0 -1\n";
    ofs << "0 0 1\n";
    ofs << "0 -1 0\n";
    ofs << "0 1 0\n";
    ofs << "-1 0 0\n";
    ofs << "1 0 0\n";

    // FIELD FieldData (Contains cellIds and faceAttributes)
    ofs << "FIELD FieldData 2\n"; // 2 arrays in FieldData

    // cellIds
    ofs << "cellIds 1 6 int\n"; // Array name, number of components (1), number of tuples (6), data type
    ofs << "0 1 2 3 4 5\n";

    // faceAttributes
    ofs << "faceAttributes 2 6 float\n"; // Array name, number of components (2), number of tuples (6), data type
    // 6 cells * 2 components/cell = 12 float values
    ofs << "0.0 1.0 1.0 2.0 2.0 3.0 3.0 4.0 4.0 5.0 5.0 6.0\n";

    // POINT_DATA
    ofs << "POINT_DATA 8\n";

    // SCALARS custom_table_scalars
    ofs << "SCALARS custom_table_scalars float 1\n";
    ofs << "LOOKUP_TABLE my_table\n";
    ofs << "0.0\n";
    ofs << "0.5\n";
    ofs << "0.5\n";
    ofs << "0.5\n";
    ofs << "0.5\n";
    ofs << "0.5\n";
    ofs << "1.0\n";
    ofs << "0.5\n";

    // SCALARS default_table_scalars
    ofs << "SCALARS default_table_scalars float 1\n";
    ofs << "LOOKUP_TABLE default\n";
    ofs << "0.0\n";
    ofs << "0.5\n";
    ofs << "0.5\n";
    ofs << "0.5\n";
    ofs << "0.5\n";
    ofs << "0.5\n";
    ofs << "1.0\n";
    ofs << "0.5\n";

    // GLOBAL LOOKUP_TABLE my_table definition
    // This table will be explicitly read and applied to the mapper
    ofs << "LOOKUP_TABLE my_table 8\n"; // Table name, number of entries
    ofs << "0.0 0.0 0.0 1.0\n"; // Black
    ofs << "0.3 0.0 0.0 1.0\n"; // Dark Red
    ofs << "0.6 0.0 0.0 1.0\n"; // Red
    ofs << "0.9 0.0 0.0 1.0\n"; // Bright Red
    ofs << "0.9 0.3 0.3 1.0\n"; // Pinkish
    ofs << "0.9 0.6 0.6 1.0\n"; // Lighter Pink
    ofs << "0.9 0.9 0.9 1.0\n"; // Light Grey
    ofs << "1.0 1.0 1.0 1.0\n"; // White

    ofs.close();
    std::cout << "Dummy VTK file '" << filename << "' created successfully." << std::endl;
}


int main(int argc, char *argv[]) {
    #if USE_DUMMY
    const std::string vtkFilename = "my_data.vtk";
    CreateDummyVtkFile(vtkFilename); // Create the dummy file first
    #else
      // PolyData file pipeline
    std::string vtkFilename = argv[1];
    if (argc > 2)
      vtkFilename = argv[2];
    #endif
    // 1. Create a PolyData Reader
    vtkNew<vtkPolyDataReader> reader;
    reader->SetFileName(vtkFilename.c_str());
    reader->Update(); // Read the file and generate polydata

    // Get the polydata object after reading
    vtkPolyData* polydata = reader->GetOutput();
    if (!polydata) {
        std::cerr << "Error: Failed to read VTK PolyData from " << vtkFilename << std::endl;
        #if USE_DUMMY
        std::remove(vtkFilename.c_str());
        #endif
        return 1;
    }

    std::cout << "\nSuccessfully loaded PolyData with " << polydata->GetNumberOfPoints()
              << " points and " << polydata->GetNumberOfCells() << " cells." << std::endl;

    // --- Prepare faceAttributes for Coloring ---
    // Get the FieldData from CellData
    vtkFieldData* cellFieldData = polydata->GetCellData();
    if (!cellFieldData) {
        std::cerr << "Error: Cell FieldData not found!" << std::endl;
        #if USE_DUMMY
        std::remove(vtkFilename.c_str());
        #endif
        return 1;
    }

    // Get the "faceAttributes" array from FieldData
    vtkDataArray* faceAttributesFieldArray = cellFieldData->GetArray("faceAttributes");
    if (!faceAttributesFieldArray) {
        std::cerr << "Error: 'faceAttributes' array not found in Cell FieldData!" << std::endl;
        #if USE_DUMMY
        std::remove(vtkFilename.c_str());
        #endif
        return 1;
    }

    // Since faceAttributes has 2 components per cell (e.g., value and something else),
    // we need to extract a single component to use for coloring with a 1D lookup table.
    // Let's use the first component (index 0) for coloring.
    vtkNew<vtkFloatArray> coloringScalars;
    coloringScalars->SetName("FaceColoringScalars");
    coloringScalars->SetNumberOfComponents(1);
    coloringScalars->SetNumberOfValues(polydata->GetNumberOfCells());

    double rangeMin = VTK_FLOAT_MAX;
    double rangeMax = VTK_FLOAT_MIN;

    for (vtkIdType i = 0; i < polydata->GetNumberOfCells(); ++i) {
        double val = faceAttributesFieldArray->GetComponent(i, 0); // Get the first component
        coloringScalars->SetValue(i, val);
        if (val < rangeMin) rangeMin = val;
        if (val > rangeMax) rangeMax = val;
    }
    std::cout << "Extracted 'FaceColoringScalars' from 'faceAttributes' component 0." << std::endl;
    std::cout << "  Scalar range for coloring: [" << rangeMin << ", " << rangeMax << "]" << std::endl;

    // Add this new scalar array to the CellData as the active scalars for coloring
    polydata->GetCellData()->SetScalars(coloringScalars);

    #if 1
    // --- Create Custom Lookup Table "my_table" ---
    vtkNew<vtkLookupTable> myCustomLookupTable;
    myCustomLookupTable->SetNumberOfTableValues(8); // As per your VTK file definition
    myCustomLookupTable->Build(); // Initialize the table

    // Set the specific RGBA values for each entry
    myCustomLookupTable->SetTableValue(0, 0.0, 0.0, 0.0, 1.0); // Black
    myCustomLookupTable->SetTableValue(1, 0.3, 0.0, 0.0, 1.0); // Dark Red
    myCustomLookupTable->SetTableValue(2, 0.6, 0.0, 0.0, 1.0); // Red
    myCustomLookupTable->SetTableValue(3, 0.9, 0.0, 0.0, 1.0); // Bright Red
    myCustomLookupTable->SetTableValue(4, 0.9, 0.3, 0.3, 1.0); // Pinkish
    myCustomLookupTable->SetTableValue(5, 0.9, 0.6, 0.6, 1.0); // Lighter Pink
    myCustomLookupTable->SetTableValue(6, 0.9, 0.9, 0.9, 1.0); // Light Grey
    myCustomLookupTable->SetTableValue(7, 1.0, 1.0, 1.0, 1.0); // White

    // Set the range for the lookup table based on the actual data's range
    // The 'faceAttributes' values are 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0
    // So the range is 0.0 to 6.0.
    myCustomLookupTable->SetRange(rangeMin, rangeMax);
    #else
    // don't get properbly color data
    vtkSmartPointer<vtkLookupTable> myCustomLookupTable = polydata->GetCellData()->GetScalars()->GetLookupTable();
    #endif
    std::cout << "Custom Lookup Table 'my_table' created and range set to [" << rangeMin << ", " << rangeMax << "]." << std::endl;


    // 2. Create a Mapper
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(polydata); // Use SetInputData because we modified polydata directly

    // --- Configure Mapper for Coloring ---
    mapper->SetScalarModeToUseCellData(); // Use cell data for coloring
    mapper->ScalarVisibilityOn();         // Ensure scalars are visible
    mapper->ColorByArrayComponent("FaceColoringScalars",1); // Color by our newly prepared scalar array
    mapper->SetLookupTable(myCustomLookupTable); // Assign our custom lookup table
    mapper->UseLookupTableScalarRangeOn(); // Crucial to use the lookup table's range for mapping

    // 3. Create an Actor
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);

    // 4. Create a Renderer
    vtkNew<vtkRenderer> renderer;
    renderer->AddActor(actor);
    vtkNew<vtkNamedColors> colors;
    renderer->SetBackground(colors->GetColor3d("SlateGrey").GetData());

    // 5. Create a Render Window
    vtkNew<vtkRenderWindow> renderWindow;
    renderWindow->AddRenderer(renderer);
    renderWindow->SetWindowName("VTK Interactive Mesh Viewer - Custom Face Coloring");
    renderWindow->SetSize(800, 600);

    // 6. Create a Render Window Interactor and set custom style
    vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
    renderWindowInteractor->SetRenderWindow(renderWindow);
    vtkNew<MyCustomInteractorStyle> style;
    renderWindowInteractor->SetInteractorStyle(style);

    // Initialize the interactor and start the event loop
    renderWindow->Render();
    renderWindowInteractor->Initialize();
    renderWindowInteractor->Start();

    // Clean up dummy file
    #if USE_DUMMY
    std::remove(vtkFilename.c_str());
    #endif

    return 0;
}
