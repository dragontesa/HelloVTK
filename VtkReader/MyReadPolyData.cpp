#include <vtkVector.h>
#include <vtkBYUReader.h>
#include <vtkOBJReader.h>
#include <vtkPLYReader.h>
#include <vtkPolyDataReader.h>
#include <vtkSTLReader.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkDataSetMapper.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkNamedColors.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtksys/SystemTools.hxx>

#include <vtkUnstructuredGrid.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkTriangleFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkLookupTable.h>
#include <vtkFloatArray.h>
#include <vtkTessellatorFilter.h>


#include <algorithm>
#include <array>
#include <random>
#include <string>
#include <vector>
#include <math.h>

// 1. Not use generic poly reader but use PolyReader
// 2. Use PolyMapper to map colors from LUT
// 3. Don't use lookuptable when use mapper
// 4. Don't use forced map when use mapper
// 5. Don't use vtk array when use mapper
#if 0 // Use PolyMapper
#define USE_GENERIC_FULL_POLY_READER 0
#define USE_POLYMAPPER 1
#define USE_MAPPER_AUTO_MAP_COLORS 1
#define USE_MAPPER_LOOKUPTABLE  0
#define USE_VTK_ARRAY_AND_NO_MAPPER 0
#else
#define USE_GENERIC_FULL_POLY_READER 0
#define USE_POLYMAPPER 0
#define USE_MAPPER_AUTO_MAP_COLORS 0
#define USE_MAPPER_LOOKUPTABLE  0
#define USE_VTK_ARRAY_AND_NO_MAPPER 0
#endif

// must be disabled
#define DRAFT_ORIGINAL_MAP_COLORS 0

#define KINDA_SMALL_NUMBER (1.e-4f)

using FVector = vtkVector<float,3>;
using FVector2D = vtkVector<float,2>;


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

vtkSmartPointer<vtkPolyData> MyReadPolyData(const char* fileName);
vtkSmartPointer<vtkLookupTable> create_lookup_table_from_vtk(const std::string& vtkFilePath, const std::string& tableName);

int main(int argc, char* argv[])
{
  // Visualize
    // 렌더러
    vtkNew<vtkNamedColors> colors;
    vtkNew<vtkRenderer> renderer;

    // 렌더 윈도우
    vtkNew<vtkRenderWindow> renderWindow;
    renderWindow->SetSize(640, 480);
    renderWindow->AddRenderer(renderer);
  
    // 윈도우 인터렉터
    vtkNew<vtkRenderWindowInteractor> interactor;
    interactor->SetRenderWindow(renderWindow);
    vtkNew<MyCustomInteractorStyle> style;
    interactor->SetInteractorStyle(style);
  
    // 카메라
    // renderer->ResetCamera();
    // renderer->SetViewport(0.0, 0.0, 1.0, 1.0); // Example: Full Window for 3D


  // support multiple pipeline
  int nBegin = 1 + (argc>2);
  for (int i = nBegin; i < argc; ++i)
  {
    std::cout << "Loading: " << argv[i] << std::endl;
    std::string vtkFileName = argv[i];
    auto polyData = MyReadPolyData(vtkFileName.c_str());

    // 1. 셀 격자화
    vtkNew<vtkTessellatorFilter> tessel;
    tessel->SetInputData(polyData);
    tessel->SetMaximumNumberOfSubdivisions(3); // 3 x 3 격자
    tessel->Update();
    auto processedData = tessel->GetOutput();
    std::cerr << "After Tessel, numcell= " << (int)processedData->GetNumberOfCells() << std::endl;

    
    // 2. 매퍼 설정
    std::string scalarName("custom_table_scalars");
    vtkNew<vtkDataSetMapper> mapper;
    mapper->SetInputData(processedData);
    mapper->SetScalarModeToUsePointData();
    mapper->SelectColorArray(scalarName.c_str());
    mapper->SetScalarVisibility(true);
    mapper->InterpolateScalarsBeforeMappingOn(); // 색상 보간
    
    // 3. 스칼라 범위
    double* scalRange = polyData->GetPointData()->GetArray(scalarName.c_str())->GetRange();
    std::cerr << "Scalar ranges: [ " << scalRange[0] << ", " << scalRange[1] << " ]" << std::endl;

    // 4. 색상 테이블
    vtkSmartPointer<vtkLookupTable> lut = create_lookup_table_from_vtk(vtkFileName,"my_table");
    lut->SetTableRange(scalRange[0], scalRange[1]);
    mapper->SetLookupTable(lut);

    // 5. 액터
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetRepresentationToSurface();
    actor->GetProperty()->EdgeVisibilityOn();
    actor->GetProperty()->SetEdgeColor(0.0, 0.0, 0.0); // # 검정 테두리
    actor->GetProperty()->SetLineWidth(2.0); //  # 테두리 두께
    // actor->GetProperty()->SetSpecular(0.3);
    // actor->GetProperty()->SetSpecularPower(30);
    // actor->SetBackfaceProperty(backProp);
    // actor->GetProperty()->SetDiffuseColor(randomColor.data());

    // 6. 렌더러에 액터 추가
    renderer->SetBackground(colors->GetColor3d("Wheat").GetData());
    renderer->UseHiddenLineRemovalOn();
    renderer->AddActor(actor);
   
  }

  renderWindow->SetWindowName("VTK Visualizer");
  renderWindow->Render();
  interactor->Start();

  return EXIT_SUCCESS;
}


vtkSmartPointer<vtkPolyData> MyReadPolyData(const char* fileName)
{
  vtkNew<vtkPolyDataReader> reader;
  reader->SetFileName(fileName);
  reader->Update();
  vtkSmartPointer<vtkPolyData> rawPoly = reader->GetOutput();

  if (!rawPoly || !rawPoly->GetPoints()) {
      std::cerr << "MyRead invalid polydata or no points" << std::endl;
      return nullptr;
  }

  // Below, It's just verify to translate everything in polydata
  vtkIdType numPoints = rawPoly->GetNumberOfPoints();
  std::cerr << "MyRead raw polydata points=" << numPoints << std::endl;

  // Ensure mesh is triangulated
  #define USE_TRY_TRIANGLED 0
  #if USE_TRY_TRIANGLED  // translate triangled poly
  vtkNew<vtkTriangleFilter> triaf;
  triaf->SetInputData(rawPoly);
  triaf->Update();

  vtkNew<vtkCleanPolyData> clean;
  clean->SetInputConnection(triaf->GetOutputPort());
  clean->Update();

  #if 0
  vtkNew<vtkPolyDataNormals> normf;
  normf->SetInputConnection(clean->GetOutputPort());
  // TODO: Need to determin API via checking what's the kind of normals if point_normals or cell_normals in .vtk file
  normf->ComputePointNormalsOff(); // explicitly turn off pointnormal, if polydata has only cell_normals like cube-colortable-correct.vtk
  normf->ComputeCellNormalsOn();
  normf->Update();
  vtkSmartPointer<vtkPolyData> poly = normf->GetOutput();
  #endif
  vtkSmartPointer<vtkPolyData> poly = clean->GetOutput();
  // final output
#else
vtkSmartPointer<vtkPolyData> poly = rawPoly;
#endif
  // 1. Vertices
  vtkPoints* points = poly->GetPoints();

  if (!points || !points->GetData())
  {
      std::cerr << "MyRead invalid points" << std::endl; // points is null, Why can't get point data through vtkPoints? but we can traverse point data via vtkPolyData's GetPoint()
      return poly;
  }

  numPoints = poly->GetNumberOfPoints();
  std::cerr << "MyRead points=" << numPoints << std::endl;

  vtkPointData* pointData = poly->GetPointData();
  vtkCellData* cellData = poly->GetCellData();
  if (!cellData)
  {
     std::cerr << "MyRead invalid celldata" << std::endl;
     return poly;
  }

  // 2. Normals
  vtkDataArray* pointNormals = poly->GetPointData()->GetNormals();
  if (!pointNormals)
  {
      std::cerr << "MyRead point normals is none" << std::endl;
    //   return poly;
  }

  vtkDataArray* cellNormals = cellData->GetNormals();
  std::cerr << "MyRead okay cell normals" << std::endl;
  if (!cellNormals)
  {
    std::cerr << "MyRead cell normals is none" << std::endl;
  }

  // 3. Scalars
  vtkDataArray* pointScalars = poly->GetPointData()->GetScalars();
  vtkUnsignedCharArray* pointColorArray = nullptr;
  if (!pointScalars)
  {
    std::cerr << "MyRead point scalars none" << std::endl;
  }
  else
  {
    std::cerr << "MyRead okay point scalars" << ": numComp="
    << pointScalars->GetNumberOfComponents() << ", numTuple="
    << pointScalars->GetNumberOfTuples() << std::endl;
    pointColorArray = vtkUnsignedCharArray::SafeDownCast(pointScalars); // Cast failed
    if (!pointColorArray) {
      std::cerr << "point color array none" << std::endl;
    }
  }

  vtkDataArray* cellScalars = cellData->GetScalars();
  vtkUnsignedCharArray* cellColorArray = nullptr;
  if (!cellScalars)
  {
    std::cerr << "MyRead cell scalars none" << std::endl;
  }
  else
  {
    std::cerr << "MyRead okay cell scalars" << ": numComp="
    << cellScalars->GetNumberOfComponents() << ", numTuple="
    << cellScalars->GetNumberOfTuples() << std::endl;
    cellColorArray = vtkUnsignedCharArray::SafeDownCast(cellScalars); // Cast failed
    if (!cellColorArray) {
      std::cerr << "cell color array none" << std::endl;
    }
    else {
      std::cerr << "MyRead okay cell color array" << ": numComp="
    << cellColorArray->GetNumberOfComponents() << ", numTuple="
    << cellColorArray->GetNumberOfTuples() << std::endl;
    }
  }

  // Cell Fields and single component
  vtkFieldData* cellField  = poly->GetCellData();
  vtkDataArray* faceAttributesFieldArray = cellField->GetArray("faceAttributes");
  if (!faceAttributesFieldArray) {
        std::cerr << "Error: 'faceAttributes' array not found in Cell FieldData!" << std::endl;
        std::remove(fileName);
        return poly;
  }

  // Since faceAttributes has 2 components per cell (e.g., value and something else),
  // we need to extract a single component to use for coloring with a 1D lookup table.
  // Let's use the first component (index 0) for coloring.
    vtkNew<vtkFloatArray> coloringScalars;
    coloringScalars->SetName("FaceColoringScalars");
    coloringScalars->SetNumberOfComponents(1);
    coloringScalars->SetNumberOfValues(poly->GetNumberOfCells());

    double rangeMin = VTK_FLOAT_MAX;
    double rangeMax = VTK_FLOAT_MIN;

    for (vtkIdType i = 0; i < poly->GetNumberOfCells(); ++i) {
        double val = faceAttributesFieldArray->GetComponent(i, 0); // Get the first component
        coloringScalars->SetValue(i, val);
        if (val < rangeMin) rangeMin = val;
        if (val > rangeMax) rangeMax = val;
    }
    std::cout << "Extracted 'FaceColoringScalars' from 'faceAttributes' component 0." << std::endl;
    std::cout << "  Scalar range for coloring: [" << rangeMin << ", " << rangeMax << "]" << std::endl;

    // Add this new scalar array to the CellData as the active scalars for coloring
    poly->GetCellData()->SetScalars(coloringScalars);
 
  
  // 3. Colors
     cellScalars = poly->GetCellData()->GetScalars();
    // vtkSmartPointer<vtkLookupTable> lut = vtkSmartPointer<vtkLookupTable>::New();
    vtkSmartPointer<vtkScalarsToColors> lut = nullptr;
    if (pointScalars) {
        lut = pointScalars->GetLookupTable();
    } else if (cellScalars) {
        lut = cellScalars->GetLookupTable();
    }

    if (!lut) {
        std::cerr << "not found lookup table from scalars" << std::endl;
        // Fallback: generate LUT manually
        vtkNew<vtkLookupTable> generatedLut;
        if (pointScalars)
            generatedLut->SetTableRange(pointScalars->GetRange());
        else if (cellScalars)
            generatedLut->SetTableRange(cellScalars->GetRange());
        generatedLut->Build();
        lut = generatedLut;
    }
     
  // Translate poly to Mesh data
  std::vector<FVector> Vertices;
  std::vector<FVector> Normals;
  std::vector<FVector2D> UVs;
  std::vector<FVector> Colors;
  std::vector<FVector> Tangents; 

  // 4. UVs 
  // compute UV projection
  double bounds[6];
  poly->GetBounds(bounds);
  double dx = bounds[1] - bounds[0]; // xMax - xMin
  double dy = bounds[3] - bounds[2]; // yMax - yMin

  // DO ALL in POINTS
    for (vtkIdType i = 0; i < numPoints; ++i) {
        // 4-1 Vertex
        double p[3];
        std::cerr << "#0 getpoint " << i << std::endl;
        poly->GetPoint(i, p);
        Vertices.push_back(vtkVector3<float>(p[0], p[1], p[2]));
        std::cerr << "#1 point= " << p[0] << ", " << p[1] << ", " << p[2] << std::endl;

        // 4-2 Vertex Normals
        if (pointNormals) {
            double n[3];
            pointNormals->GetTuple(i, n);
            Normals.push_back(vtkVector3<float>(n[0], n[1], n[2]));
            std::cerr << "#2 point normal= "<< n[0] << ", " << n[1] << ", " << n[2] << std::endl;
        } else {
            Normals.push_back(vtkVector3<float>(0.0f,1.0f,0.0f));
            std::cerr << "#2 point normal" << std::endl;
        }

        // 4-3 Vertex UVs
        float u = static_cast<float>((p[0]-bounds[0])/dx);
        float v = static_cast<float>((p[1]-bounds[2])/dy);
        UVs.push_back(vtkVector2<float>(u,v));
        
        // 4-4 Vertex COLOR
        FVector color(vtkVector3<float>(0.5,0.5,0.5));
        if (pointScalars) {
            double val = pointScalars->GetTuple1(i);
            double rgb[3];
            lut->GetColor(val, rgb);
            color[0] = rgb[0];
            color[1] = rgb[1];
            color[2] = rgb[2];
            std::cerr << "#3 point color= " 
                << color(0) << ", "
                << color(1) << ", "
                << color(2) << std::endl;
        }

        Colors.push_back(color);

        // 4-5 TANGENT
        Tangents.push_back(vtkVector3<float>(1.0f, 0.0f, 0.0f)); // placeholder 
    }

    std::cerr << "MyReader POLY Points End !" << std::endl;


    // 5. DO ALL in Cell(Face)
    //  5-1 TRIANGLES(INDEX)
    //  5-2 Cell NORMAL
    //  5-3 Cell Colors triangles 
    //  5-4 Tangent
    // compute triangles and tangents from cell data
    vtkCellArray* cells = poly->GetPolys();
    vtkIdType npts;
    const vtkIdType* ptIds;
    vtkIdType cellId = 0;
    std::vector<vtkIdType> Triangles;

    for (cells->InitTraversal(); cells->GetNextCell(npts, ptIds);++cellId) {
        if (npts == 3) { // only triangled polys
           // 5-1 TRIANGLES
            std::cerr << "MyReader traverse cell "
            << cellId << ", pts "
            << ptIds[0] << ", "
            << ptIds[1] << ", "
            << ptIds[2] << std::endl;

            Triangles.push_back(ptIds[0]);
            Triangles.push_back(ptIds[1]);
            Triangles.push_back(ptIds[2]);

            // 5-2 CeLL NORMAL
            FVector normal(vtkVector3<float>(0.0f,1.0f,0.0f));
            if (!pointNormals && cellNormals) {
                double n[3];
                cellNormals->GetTuple(cellId, n);
                // normal = FVector(n[0], n[1], n[2]);
                normal[0] = n[0]; // x
                normal[1] = n[1]; // y
                normal[2] = n[2]; // z
                Normals[ptIds[0]] = normal;
                Normals[ptIds[1]] = normal;
                Normals[ptIds[2]] = normal;
                std::cerr << "#2 cell normal= "<< n[0] << ", " << n[1] << ", " << n[2] << std::endl;
            }

            // 5-3 CELL COLOR 
            FVector color(vtkVector3<float>(0.5,0.5,0.5));
            if (!pointScalars && cellScalars) {
                double val = cellScalars->GetTuple1(cellId);
                double rgb[3];
                lut->GetColor(val, rgb);
                color[0] = rgb[0];
                color[1] = rgb[1];
                color[2] = rgb[2];
                std::cerr << "#3 cell color= " 
                << color(0) << ", "
                << color(1) << ", "
                << color(2) << std::endl;

                Colors[ptIds[0]] = color;
                Colors[ptIds[1]] = color;
                Colors[ptIds[2]] = color;
            }

            // 5-4 TANGENT
            // Compute tangents from trpiangle edges
            FVector& p0 = Vertices[ptIds[0]];
            FVector& p1 = Vertices[ptIds[1]];
            FVector& p2 = Vertices[ptIds[2]];

            FVector2D& uv0 = UVs[ptIds[0]];
            FVector2D& uv1 = UVs[ptIds[1]];
            FVector2D& uv2 = UVs[ptIds[2]];

            FVector edge1 = p1 - p0;
            FVector edge2 = p2 - p0;
            FVector2D deltaUV1 = uv1 - uv0;
            FVector2D deltaUV2 = uv2 - uv0;

            float df = deltaUV1(0) * deltaUV2(1) - deltaUV2(0) * deltaUV1(1);

            if (abs(df) < KINDA_SMALL_NUMBER) df = 1.0f; else df = 1.0f / df;
            // FVector tangent = df * (deltaUV2(0) * edge1 - deltaUV1(1) * edge2);
            FVector tangent(df * (deltaUV2(0) * edge1 - deltaUV1(1) * edge2));
            tangent.Normalize();

            Tangents[ptIds[0]] = tangent;
            Tangents[ptIds[1]] = tangent;
            Tangents[ptIds[2]] = tangent;

            std::cerr   << "MyReader tangents " << ptIds[0] << ", " 
            << ptIds[1] << ", " 
            << ptIds[2] << "'s tangent= " 
            << tangent(0) << ", " 
            << tangent(1) << ", " 
            << tangent(2) << std::endl;
        }
    }

    return poly;
}
