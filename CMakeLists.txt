cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
project(HelloVtk)

# Include .cmake for your target example 
include(vtk.cmake)

add_subdirectory(VtkReader)
add_executable(HelloVtk hellovtk.cpp)

target_compile_features(HelloVtk PUBLIC cxx_std_20)


# vtk_module_autoinit is needed
vtk_module_autoinit(
  TARGETS ${PROJECT_NAME}
  MODULES ${VTK_LIBRARIES}
)
