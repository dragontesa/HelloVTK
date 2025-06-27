#include <stdio.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <format>
#include <vector>
#include <vtkSmartPointer.h>
#include <vtkLookupTable.h>

vtkSmartPointer<vtkLookupTable> 
create_lookup_table_from_vtk(const std::string& vtkFilePath, const std::string& tableName)
{
    std::ifstream fs(vtkFilePath,std::ios_base::in);
    if (!fs) {
       std::cerr << "Error: Could not open VTK file: " << vtkFilePath << std::endl;
       return nullptr;
    }

    std::string line;
    int numEntries = 0;
    std::vector<std::array<float, 4>> tableData;
    bool foundTable = false;

    std::string lutName = std::format("LOOKUP_TABLE {}", tableName);
    while (std::getline(fs,line)) {
        if (!foundTable && line.find(lutName) != std::string::npos)
        {
            std::istringstream is(line);
            std::string token;
            std::vector<std::string> elements;
            while (is >> token) {
                elements.push_back(token);
            }

            if (elements.size() >= 3) {
                try {
                    numEntries = atoi(elements.at(elements.size() - 1).c_str());
                    foundTable = true;
                }
                catch (const std::exception &e) {
                    std::cerr << "Error, lookup table could not parse number of entries" << e.what() << std::endl;
                }
            }
            continue;
        }

        // extract each data in lookup table 
        if (foundTable && line.length() > 0)
        {
           std::istringstream is(line);
           std::string token;
           std::vector<std::string> elements;
           while (is >> token) {
               elements.push_back(token);
           }
           if (elements.size() == 4) {
               try {
                  std::array<float,4> rgba;
                  rgba[0] = atof(elements.at(0).c_str());
                  rgba[1] = atof(elements.at(1).c_str());
                  rgba[2] = atof(elements.at(2).c_str());
                  rgba[3] = atof(elements.at(3).c_str());
                  tableData.push_back(rgba);
               }
               catch (const std::exception& e) {
                   std::cerr << "Error, lookup table could not parse data" << e.what() << std::endl;
               }
           }
        }
    }

    vtkSmartPointer<vtkLookupTable> lut = vtkSmartPointer<vtkLookupTable>::New();
    // if it gets failed, create empty LookupTable
    if (tableData.empty() || tableData.size() != numEntries)
    {
        std::cerr << "Error, lookuptable data " 
        << tableData.size() << " not macthed to "
        << numEntries << " which expected" <<  std::endl;
        lut->SetNumberOfTableValues(256);
        lut->SetHueRange(0.667f, 0.0f); // 파랑 -> 빨강
        lut->Build();
        return lut;
    }

    lut->SetNumberOfTableValues(numEntries);
    int i=0;
    for (const auto& rgba: tableData)
    {
        lut->SetTableValue(i++, rgba[0], rgba[1], rgba[2], rgba[3]);
    }
    lut->Build();

    return lut;

}