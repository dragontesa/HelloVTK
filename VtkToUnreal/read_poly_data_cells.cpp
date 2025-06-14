// Reads and prints most cell types (verts, lines, polys, strips) from a vtkPolyData
#include <vtkSmartPointer.h>
#include <vtkPolyDataReader.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkIdList.h>
#include <iostream>

void PrintCellArray(vtkCellArray* cells, const std::string& label) {
    vtkIdType npts;
    const vtkIdType* ptIds;

    std::cout << label << ":\n";
    for (cells->InitTraversal(); cells->GetNextCell(npts, ptIds);) {
        std::cout << "  Cell with " << npts << " points: ";
        for (vtkIdType i = 0; i < npts; ++i) {
            std::cout << ptIds[i] << " ";
        }
        std::cout << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.vtk>" << std::endl;
        return EXIT_FAILURE;
    }

    vtkNew<vtkPolyDataReader> reader;
    reader->SetFileName(argv[1]);
    reader->Update();

    vtkPolyData* polyData = reader->GetOutput();
    if (!polyData) {
        std::cerr << "Failed to read vtkPolyData." << std::endl;
        return EXIT_FAILURE;
    }

    vtkPoints* points = polyData->GetPoints();
    std::cout << "Points: " << points->GetNumberOfPoints() << "\n";
    for (vtkIdType i = 0; i < points->GetNumberOfPoints(); ++i) {
        double p[3];
        points->GetPoint(i, p);
        std::cout << "  Point " << i << ": (" << p[0] << ", " << p[1] << ", " << p[2] << ")\n";
    }

    PrintCellArray(polyData->GetVerts(), "Verts");
    PrintCellArray(polyData->GetLines(), "Lines");
    PrintCellArray(polyData->GetPolys(), "Polys");
    PrintCellArray(polyData->GetStrips(), "Strips");

    return EXIT_SUCCESS;
}
