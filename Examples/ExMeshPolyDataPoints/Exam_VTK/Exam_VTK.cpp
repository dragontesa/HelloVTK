// Exam_VTK.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include <vtkAutoInit.h>
#define vtkRenderingCore_AUTOINIT 3(vtkRenderingOpenGL2,vtkInteractionStyle, vtkRenderingFreeType)
#define vtkRenderingContext2D_AUTOINIT 1(vtkRenderingContextOpenGL2)

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkCellLocator.h>
#include <vtkDelaunay2D.h>
#include <vtkLine.h>
#include <vtkNamedColors.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkTriangle.h>
#include <vtkVertexGlyphFilter.h>
#include <vtkPLYReader.h>

int main(int, char*[])
{
	vtkNew<vtkNamedColors> colors;

	vtkNew<vtkPLYReader> reader;
	reader->SetFileName("test.ply");
	reader->Update();
	
	vtkNew<vtkPoints> points;
	points->DeepCopy(reader->GetOutput()->GetPoints());

	// vtk data set to fill
	vtkNew<vtkPolyData> myPolyData;

	// compute a vtkIdList that contains ids of each points
	vtkIdType numPoints = points->GetNumberOfPoints();
	vtkSmartPointer<vtkIdList> pointIds = vtkSmartPointer<vtkIdList>::New();
	pointIds->SetNumberOfIds(numPoints);
	for (vtkIdType i = 0; i < numPoints; ++i)
	{
		pointIds->SetId(i, i);
	}

	// create a vtkCellArray from this list
	vtkSmartPointer<vtkCellArray> polyPoint = vtkSmartPointer<vtkCellArray>::New();
	polyPoint->InsertNextCell(pointIds);

	// give the points and cells to the final poly data
	myPolyData->SetPoints(points);
	myPolyData->SetVerts(polyPoint);

	// Triangulate the grid points
	vtkNew<vtkDelaunay2D> delaunay;
	delaunay->SetInputData(myPolyData);

	// Visualize
	vtkNew<vtkPolyDataMapper> meshMapper;
	meshMapper->SetInputConnection(delaunay->GetOutputPort());

	vtkNew<vtkActor> meshActor;
	meshActor->SetMapper(meshMapper);
	meshActor->GetProperty()->SetColor(colors->GetColor3d("Banana").GetData());
	meshActor->GetProperty()->EdgeVisibilityOn();

	vtkNew<vtkVertexGlyphFilter> glyphFilter;
	glyphFilter->SetInputData(myPolyData);

	vtkNew<vtkPolyDataMapper> pointMapper;
	pointMapper->SetInputConnection(glyphFilter->GetOutputPort());

	vtkNew<vtkActor> pointActor;
	pointActor->GetProperty()->SetColor(colors->GetColor3d("Tomato").GetData());
	pointActor->GetProperty()->SetPointSize(5);
	pointActor->SetMapper(pointMapper);

	vtkNew<vtkRenderer> renderer;
	vtkNew<vtkRenderWindow> renderWindow;
	renderWindow->AddRenderer(renderer);
	vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
	renderWindowInteractor->SetRenderWindow(renderWindow);

	renderer->AddActor(meshActor);
	renderer->AddActor(pointActor);
	renderer->SetBackground(colors->GetColor3d("Mint").GetData());

	renderWindow->SetWindowName("Delaunay2D");
	renderWindow->Render();
	renderWindowInteractor->Start();

	return EXIT_SUCCESS;
}