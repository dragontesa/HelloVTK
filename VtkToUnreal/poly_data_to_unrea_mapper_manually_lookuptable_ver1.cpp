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

vtkSmartPointer<vtkLookupTable> ExtractLookupTableFromPolyData(vtkPolyData* poly, const std::string& lutName)
{
    // PolyData 의 Field를 사용할 경우, cube-colortable-correct.vtk 의 Polydata의 Lookup Table을
    // 찾을 수 없다, 그래서 Mapper를 이용해서 Lookup table을 찾는 Version 2를 사용
    vtkSmartPointer<vtkLookupTable> lut = vtkSmartPointer<vtkLookupTable>::New();
    vtkFieldData* fieldData = poly->GetFieldData();
    if (!fieldData)
    {
        std::cerr << "No field data in polydata." << std::endl;
        return nullptr;
    }

    for (int i = 0; i < fieldData->GetNumberOfArrays(); ++i)
    {
        vtkDataArray* array = fieldData->GetArray(i);
        if (!array || !array->GetName())
            continue;

        std::string name(array->GetName());
        if (name == lutName)
        {
            vtkFloatArray* lutArray = vtkFloatArray::SafeDownCast(array);
            if (!lutArray)
            {
                std::cerr << "Found LUT by name but not float array." << std::endl;
                return nullptr;
            }

            vtkIdType numTuples = lutArray->GetNumberOfTuples();
            lut->SetNumberOfTableValues(numTuples);
            lut->Build();
            for (vtkIdType j = 0; j < numTuples; ++j)
            {
                double rgba[4] = {1.0, 1.0, 1.0, 1.0};
                lutArray->GetTuple(j, rgba);
                lut->SetTableValue(j, rgba);
            }
            return lut;
        }
    }

    std::cerr << "LUT named " << lutName << " not found in field data arrays." << std::endl;
    return nullptr;
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

    vtkSmartPointer<vtkLookupTable> lut = ExtractLookupTableFromPolyData(poly, lookupTableName);
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

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(poly);
    mapper->SetScalarVisibility(true);
    mapper->SetLookupTable(lut);
    mapper->SetColorModeToMapScalars();
    mapper->SetScalarMode(useCellData ? VTK_SCALAR_MODE_USE_CELL_DATA : VTK_SCALAR_MODE_USE_POINT_DATA);
    mapper->SelectColorArray(scalarName.c_str());
    mapper->Update();

    return mapper->MapScalars(1.0);
}

// Example usage:
// vtkUnsignedCharArray* mappedColors = MapScalarsWithCustomLUT(poly, "custom_table_scalars", "my_table", false);
