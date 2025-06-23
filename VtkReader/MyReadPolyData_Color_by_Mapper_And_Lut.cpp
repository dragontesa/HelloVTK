#include <vtkVector.h>
#include <vtkBYUReader.h>
#include <vtkOBJReader.h>
#include <vtkPLYReader.h>
#include <vtkPolyDataReader.h>
#include <vtkSTLReader.h>
#include <vtkXMLPolyDataReader.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkNamedColors.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtksys/SystemTools.hxx>

#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkTriangleFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkLookupTable.h>

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
#if 0 // use PolyMapper
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
namespace {

vtkSmartPointer<vtkPolyData> MyReadPolyData(const char* fileName);

}


int main(int argc, char* argv[])
{
      // Vis Pipeline
  vtkNew<vtkNamedColors> colors;

  vtkNew<vtkRenderer> renderer;

  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->SetSize(640, 480);
  renderWindow->AddRenderer(renderer);

  vtkNew<vtkRenderWindowInteractor> interactor;
  interactor->SetRenderWindow(renderWindow);

  // renderer->SetViewport(0.0, 0.0, 1.0, 1.0); // Example: Full Window for 3D
  renderer->SetBackground(colors->GetColor3d("Wheat").GetData());
  renderer->UseHiddenLineRemovalOn();

  // Note: If a Python version is written, it is probably best to use
  //       vtkMinimalStandardRandomSequence in it and here, to ensure
  //       that the random number generation is the same.
  std::mt19937 mt(4355412); // Standard mersenne_twister_engine
  std::uniform_real_distribution<double> distribution(0.6, 1.0);

  argc = 2;
  argv[1] = const_cast<char*>("/home/dragontesa/314/etri/data/vtk/legacy/cube-colortable-correct.vtk");

  // PolyData file pipeline
  for (int i = 1; i < argc; ++i)
  {
    std::cout << "Loading: " << argv[i] << std::endl;
    auto polyData = MyReadPolyData(argv[i]);

    // Visualize
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(polyData);

    std::array<double, 3> randomColor;
    randomColor[0] = distribution(mt);
    randomColor[1] = distribution(mt);
    randomColor[2] = distribution(mt);
    vtkNew<vtkProperty> backProp;
    backProp->SetDiffuseColor(colors->GetColor3d("Banana").GetData());
    backProp->SetSpecular(0.6);
    backProp->SetSpecularPower(30);

    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->SetBackfaceProperty(backProp);
    actor->GetProperty()->SetDiffuseColor(randomColor.data());
    actor->GetProperty()->SetSpecular(0.3);
    actor->GetProperty()->SetSpecularPower(30);
    renderer->AddActor(actor);
  }

  renderWindow->SetWindowName("ReadAllPolyDataTypes");
  renderWindow->Render();
  interactor->Start();

  return EXIT_SUCCESS;
}


namespace {
vtkUnsignedCharArray* MapScalarsFromPolyData(
    vtkPolyData* poly,
    const char* scalarName,
    bool useCellData
);

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

  vtkIdType numPoints = rawPoly->GetNumberOfPoints();
  std::cerr << "MyRead raw polydata points=" << numPoints << std::endl;

  // Ensure mesh is triangulated
  vtkNew<vtkTriangleFilter> triaf;
  triaf->SetInputData(rawPoly);
  triaf->Update();

  vtkNew<vtkCleanPolyData> clean;
  clean->SetInputConnection(triaf->GetOutputPort());
  clean->Update();

  vtkNew<vtkPolyDataNormals> normf;
  normf->SetInputConnection(clean->GetOutputPort());
  // TODO: Need to determin API via checking what's the kind of normals if point_normals or cell_normals in .vtk file
  normf->ComputePointNormalsOff(); // explicitly turn off pointnormal, if polydata has only cell_normals like cube-colortable-correct.vtk
  normf->ComputeCellNormalsOn();
  normf->Update();

  // final output
  vtkSmartPointer<vtkPolyData> poly = normf->GetOutput();

  // 1. Vertices
  vtkPoints* points = poly->GetPoints();

  if (!points || !points->GetData())
  {
      std::cerr << "MyRead invalid points after filter" << std::endl; // points is null, Why can't get point data through vtkPoints? but we can traverse point data via vtkPolyData's GetPoint()
      return poly;
  }

  numPoints = poly->GetNumberOfPoints();
  std::cerr << "MyRead points=" << numPoints << " after filter" << std::endl;

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
    pointColorArray = vtkUnsignedCharArray::SafeDownCast(pointScalars);
  }

  vtkDataArray* cellScalars = cellData->GetScalars();
  vtkUnsignedCharArray* cellColorArray = nullptr;
  std::cerr << "MyRead okay cell scalars" << std::endl;
  if (!cellScalars)
  {
    std::cerr << "MyRead cell scalars none" << std::endl;
  }
  else
  {
    cellColorArray = vtkUnsignedCharArray::SafeDownCast(cellScalars);
  }
  
  // 3. Colors
  #if USE_POLYMAPPER  // mapper map the named scalars from lookup table.
  vtkUnsignedCharArray* mappedColorData = MapScalarsFromPolyData(poly, "custom_table_scalars", false);
  if (mappedColorData) {
        vtkIdType numTuple = mappedColorData->GetNumberOfTuples();
        int numComp = mappedColorData->GetNumberOfComponents();
        std::cerr<<"MyRead mapped color numComp="<<numComp<<", numTuple=" << numTuple << std::endl;
        for (vtkIdType i=0;i<numTuple;i++)
        {
            #if USE_MAPPER_AUTO_MAP_COLORS
            unsigned char colors[4];
            mappedColorData->GetTypedTuple(i,colors); // Crash!
            std::cerr << "color= "<< colors[0] << ", " << colors[1] << ", " << colors[2] << std::endl;
            #else
               float colors[4];
               #if USE_VTK_ARRAY_AND_NO_MAPPER
                   mappedColorData->GetTypedTuple(i,colors);
                #else
                   mappedColorData->GetTuple(i,colors);
                #endif
                
            std::cerr << "coloXr= "<< colors[0] << ", " << colors[1] << ", " << colors[2] << std::endl;
            #endif
            // std::cerr << "color = " << colors[j] << ", ";
            // std::cerr << std::endl;
        }
    }
    else {
          std::cerr << "mapped color none !!!!" << std::endl;
    }
    #else
    vtkSmartPointer<vtkLookupTable> lut = vtkSmartPointer<vtkLookupTable>::New();
    if (cellScalars) {
             lut->SetTableRange(cellScalars->GetRange());
             lut->Build();
    } else if (pointScalars) {
             lut->SetTableRange(pointScalars->GetRange());
             lut->Build();
    }
    
    #endif

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
        #if USE_POLYMAPPER
            if (mappedColorData && i < mappedColorData->GetNumberOfTuples())
            {
                #if USE_MAPPER_AUTO_MAP_COLORS // exactly using mapper
                    unsigned char rgb[3];
                    mappedColorData->GetTypedTuple(i, rgb);
                    color[0] = rgb[0] / 255.0f; // r
                    color[1] = rgb[1] / 255.0f; // g
                    color[2] = rgb[2] / 255.0f; // b
                    // color = vtkVector3<float>(rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f);
                #else
                   float rgba[4];
                   #if USE_VTK_ARRAY_AND_NO_MAPPER
                    mappedColorData->GetTypedTuple(i, rgba);
                    #else
                    mappedColorData->GetTuple(i, rgba);
                    #endif
                    color = FVector(rgba[0], rgba[1], rgba[2], rgba[3]);
                #endif
            std::cerr <<"color= "<< color(0) << ", " << color(1) << ", " << color(2) << std::endl;
            }
        #else
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

        #endif
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
            std::cerr << "PolyTransfer cell "
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
            #if USE_POLYMAPPER
            #else
            if (cellScalars) {
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
            #endif

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

vtkUnsignedCharArray* MapScalarsFromPolyData(vtkPolyData* poly, const char* scalarName, bool useCellData)
{
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(poly);
    mapper->SetScalarVisibility(true);
    mapper->SetScalarMode(useCellData ? VTK_SCALAR_MODE_USE_CELL_DATA : VTK_SCALAR_MODE_USE_POINT_DATA);  // default is VTK_SCALAR_MODE_USE_POINT_DATA, vtk defect automatically cell data if there is not point data
    // mapper->SetScalarModeToUsePointFieldData(); // No Effects
    mapper->SetColorModeToMapScalars();
    // mapper->SelectColorArray(scalarName);
    mapper->Update();

    vtkUnsignedCharArray* mappedScalars = mapper->MapScalars(1.0);
    std::cerr << "mapper 2 >> numcomp="
    << mappedScalars->GetNumberOfComponents() << ", numtuple="
    << mappedScalars->GetNumberOfTuples() << std::endl;

    return mapper->MapScalars(1.0);
}

}