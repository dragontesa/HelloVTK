// Converts 3D vtkPolyData (.vtk) directly into an Unreal Engine mesh, supporting cell- and point-based scalar color lookup tables
#include <vtkSmartPointer.h>
#include <vtkPolyDataReader.h>
#include <vtkTriangleFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkLookupTable.h>
#include <vtkScalarsToColors.h>
#include <vtkUnsignedCharArray.h>
#include <vtkFieldData.h>
#include <vtkObjectFactory.h>
#include <vtkGenericDataObjectReader.h>
#include <vtkFloatArray.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkPolyDataMapper.h>
#include <vtkStringArray.h>
#include <vtkAbstractArray.h>
#include <iostream>

#include "ProceduralMeshComponent.h"

vtkSmartPointer<vtkLookupTable> ExtractLookupTableFromMapper(vtkPolyDataMapper* mapper)
{
    vtkScalarsToColors* scalarsToColors = mapper->GetLookupTable();
    if (!scalarsToColors)
    {
        std::cerr << "Mapper has no lookup table." << std::endl;
        return nullptr;
    }

    vtkLookupTable* lut = vtkLookupTable::SafeDownCast(scalarsToColors);
    if (!lut)
    {
        std::cerr << "Lookup table is not of type vtkLookupTable." << std::endl;
        return nullptr;
    }

    vtkSmartPointer<vtkLookupTable> copiedLUT = vtkSmartPointer<vtkLookupTable>::New();
    copiedLUT->DeepCopy(lut);
    return copiedLUT;
}

vtkUnsignedCharArray* MapScalarsWithCustomLUT(vtkPolyData* poly, const std::string& scalarName, const std::string& lookupTableName, bool useCellData)
{
    vtkSmartPointer<vtkDataArray> scalars = useCellData
        ? poly->GetCellData()->GetArray(scalarName.c_str())
        : poly->GetPointData()->GetArray(scalarName.c_str());

    if (!scalars) {
        std::cerr << "No scalar array found: " << scalarName << std::endl;
        return nullptr;
    }

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(poly);
    mapper->SetScalarVisibility(true);
    mapper->SetColorModeToMapScalars();
    mapper->SetScalarMode(useCellData ? VTK_SCALAR_MODE_USE_CELL_DATA : VTK_SCALAR_MODE_USE_POINT_DATA);
    mapper->SelectColorArray(scalarName.c_str());
    mapper->Update();

    vtkSmartPointer<vtkLookupTable> lut = ExtractLookupTableFromMapper(mapper);
    if (!lut)
    {
        std::cerr << "Falling back to default hardcoded LUT." << std::endl;
        lut = vtkSmartPointer<vtkLookupTable>::New();
        lut->SetNumberOfTableValues(8);
        lut->Build();
        lut->SetTableValue(0, 0.0, 0.0, 0.0, 1.0);
        lut->SetTableValue(1, 0.3, 0.0, 0.0, 1.0);
        lut->SetTableValue(2, 0.6, 0.0, 0.0, 1.0);
        lut->SetTableValue(3, 0.9, 0.0, 0.0, 1.0);
        lut->SetTableValue(4, 0.9, 0.3, 0.3, 1.0);
        lut->SetTableValue(5, 0.9, 0.6, 0.6, 1.0);
        lut->SetTableValue(6, 0.9, 0.9, 0.9, 1.0);
        lut->SetTableValue(7, 1.0, 1.0, 1.0, 1.0);
    }

    mapper->SetLookupTable(lut);

    return mapper->MapScalars(1.0);
}

// Example usage:
// vtkUnsignedCharArray* mappedColors = MapScalarsWithCustomLUT(poly, "custom_table_scalars", "my_table", false);
