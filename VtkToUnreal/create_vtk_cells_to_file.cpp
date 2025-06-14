// Creates vtkCellArray data for verts, lines, and polys manually and saves to .vtp file
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkVertex.h>
#include <vtkLine.h>
#include <vtkPolygon.h>
#include <vtkXMLPolyDataWriter.h>
#include <iostream>

int main() {
    vtkNew<vtkPoints> points;
    points->InsertNextPoint(0.0, 0.0, 0.0);
    points->InsertNextPoint(1.0, 0.0, 0.0);
    points->InsertNextPoint(1.0, 1.0, 0.0);
    points->InsertNextPoint(0.0, 1.0, 0.0);

    vtkNew<vtkCellArray> verts;
    for (vtkIdType i = 0; i < points->GetNumberOfPoints(); ++i) {
        verts->InsertNextCell(1);
        verts->InsertCellPoint(i);
    }

    vtkNew<vtkCellArray> lines;
    vtkIdType linePts[] = {0, 1, 2};
    lines->InsertNextCell(3, linePts);

    vtkNew<vtkCellArray> polys;
    vtkIdType polyPts[] = {0, 1, 2, 3};
    polys->InsertNextCell(4, polyPts);

    vtkNew<vtkPolyData> polyData;
    polyData->SetPoints(points);
    polyData->SetVerts(verts);
    polyData->SetLines(lines);
    polyData->SetPolys(polys);

    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName("output.vtp");
    writer->SetInputData(polyData);
    writer->Write();

    std::cout << "PolyData written to output.vtp\n";
    return EXIT_SUCCESS;
}
