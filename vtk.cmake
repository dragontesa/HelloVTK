cmake_minimum_required(VERSION 3.20)

# cmake -DVTK_DIR=/path/to/your/vtk/installation/lib/cmake/vtk-9.0 ..
set(VTK_DIR "/opt/vtk/9.4.1/lib64/cmake/vtk-9.4")
# set(CMAKE_MODULE_PATH "${VTK_DIR}/lib64/cmake/vtk-9.4")
set(VTK_INCLUDE "/opt/vtk/9.4.1/include/vtk-9.4")
set(VTK_LIBS "/opt/vtk/9.4.1/lib64")

find_package(VTK COMPONENTS 
  CommonColor
  CommonCore
  FiltersSources
  IOGeometry
  IOLegacy
  IOPLY
  IOXML
  InteractionStyle
  RenderingContextOpenGL2
  RenderingCore
  RenderingFreeType
  RenderingGL2PSOpenGL2
  RenderingOpenGL2
)

# Prevent a "command line is too long" failure in Windows.
# set(CMAKE_NINJA_FORCE_RESPONSE_FILE "ON" CACHE BOOL "Force Ninja to use response files.")
# message(FATAL_ERROR "${PROJECT_NAME}: hello vtk project")

if (NOT VTK_FOUND)
  message(FATAL_ERROR "${PROJECT_NAME}: Unable to find the VTK build folder.")
endif()

