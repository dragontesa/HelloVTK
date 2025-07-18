cmake_minimum_required(VERSION 3.12)
project(VTKInteractiveMesh CXX)

find_package(VTK COMPONENTS
    vtkCommonCore
    vtkCommonDataModel
    vtkFiltersSources
    vtkInteractionStyle
    vtkRenderingCore
    vtkRenderingOpenGL2
    vtkFiltersModeling # For vtkNamedColors
    vtkCommonColor # For vtkNamedColors
REQUIRED)

if (VTK_FOUND)
    include(${VTK_USE_FILE})
    add_executable(${PROJECT_NAME} vtk_interactive_mesh.cpp)
    target_link_libraries(${PROJECT_NAME} ${VTK_LIBRARIES})
endif()