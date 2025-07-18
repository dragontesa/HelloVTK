#include <vtkPolyDataReader.h> // For reading the .vtk file
#include <vtkPolyData.h>       // To get the PolyData object
#include <vtkCellArray.h>      // The class we want to traverse
#include <vtkIdList.h>         // To store the point IDs of each cell
#include <vtkNew.h>            // For vtkNew smart pointers

#include <iostream>
#include <string>
#include <fstream> // For creating the dummy VTK file
#include <vector>

// --- Function to create the dummy .vtk file (from previous example) ---
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

    // CELL_DATA (for completeness, though not used in this traversal example)
    ofs << "CELL_DATA 6\n";
    ofs << "SCALARS cell_scalars int 1\n";
    ofs << "LOOKUP_TABLE default\n";
    ofs << "0\n";
    ofs << "1\n";
    ofs << "2\n";
    ofs << "3\n";
    ofs << "4\n";
    ofs << "5\n";

    ofs.close();
    std::cout << "Dummy VTK file '" << filename << "' created successfully." << std::endl;
}

int main(int, char *[]) {
    const std::string vtkFilename = "my_data_for_traversal.vtk";
    CreateDummyVtkFile(vtkFilename); // Create the dummy file first

    // 1. Read the PolyData file
    vtkNew<vtkPolyDataReader> reader;
    reader->SetFileName(vtkFilename.c_str());
    reader->Update(); // Read the file and generate polydata

    vtkSmartPointer<vtkPolyData> polydata = reader->GetOutput();
    if (!polydata) {
        std::cerr << "Error: Failed to read VTK PolyData from " << vtkFilename << std::endl;
        std::remove(vtkFilename.c_str());
        return 1;
    }

    std::cout << "\nSuccessfully loaded PolyData with " << polydata->GetNumberOfPoints()
              << " points and " << polydata->GetNumberOfCells() << " cells." << std::endl;

    // 2. Get the vtkCellArray containing the polygons (faces)
    // For PolyData, polygons are stored in the GetPolys() vtkCellArray.
    // Lines, Vertices, and Strips are in their respective vtkCellArrays.
    vtkCellArray* polygons = polydata->GetPolys();

    if (!polygons) {
        std::cerr << "Error: No polygons found in the PolyData!" << std::endl;
        std::remove(vtkFilename.c_str());
        return 1;
    }

    std::cout << "\n--- Traversing Cells in vtkCellArray ---" << std::endl;

    // 3. Initialize traversal
    // This prepares the vtkCellArray for sequential reading.
    polygons->InitTraversal();

    vtkNew<vtkIdList> idList; // A vtkIdList will store the point IDs for each cell
    int cellCount = 0;

    // 4. Loop through cells using GetNextCell()
    // GetNextCell() returns true if a cell was successfully retrieved, false otherwise.
    // It populates the provided vtkIdList with the point IDs of the current cell.
    while (polygons->GetNextCell(idList)) {
        cellCount++;
        std::cout << "  Cell " << cellCount << " (Number of Points: " << idList->GetNumberOfIds() << "): ";

        // 5. Iterate through the point IDs in the current cell
        // idList->GetId(i) gives you the global index of the point for this cell.
        for (vtkIdType i = 0; i < idList->GetNumberOfIds(); ++i) {
            std::cout << idList->GetId(i);
            if (i < idList->GetNumberOfIds() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << std::endl;
    }

    std::cout << "\nFinished traversing " << cellCount << " cells." << std::endl;

    // Clean up dummy file
    std::remove(vtkFilename.c_str());

    return 0;
}

/**
 
```

### Explanation of the Traversal Method:

1.  **`polygons->InitTraversal();`**:
    * This function initializes an internal pointer within the `vtkCellArray` to the beginning of its data. It's a mandatory first step before you start reading cells sequentially.

2.  **`vtkNew<vtkIdList> idList;`**:
    * `vtkIdList` is a helper class in VTK specifically designed to hold lists of integer IDs (like point indices). You'll reuse this single `vtkIdList` object for each cell. `GetNextCell()` will clear and repopulate it for every new cell.

3.  **`while (polygons->GetNextCell(idList))`**:
    * This is the main loop. `GetNextCell(idList)` attempts to retrieve the next cell's connectivity information.
    * If a cell is successfully retrieved, it populates `idList` with the global point IDs that define that cell, and the function returns `true`.
    * If there are no more cells (or an error occurs), it returns `false`, terminating the loop.

4.  **`idList->GetNumberOfIds()`**:
    * Inside the loop, `idList->GetNumberOfIds()` tells you how many points (vertices) are in the current cell. For a quad, it will be 4; for a triangle, 3.

5.  **`idList->GetId(i)`**:
    * Use this to get the `i`-th point ID for the current cell. This ID is the global index of the point in the `vtkPoints` object of your `vtkPolyData`.

This method is efficient because it avoids creating new `vtkIdList` objects for every cell and directly accesses the pre-parsed data within the `vtkCellArray`. It's the recommended way for most cell-by-cell processing in V


* 
*/