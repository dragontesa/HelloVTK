# add_executable(${PROJECT_NAME} MACOSX_BUNDLE ReadAllPolyDataTypes.cxx )
#   target_link_libraries(ReadAllPolyDataTypes PRIVATE ${VTK_LIBRARIES}
# )

add_executable(VtkReader)
add_executable(MyReadPolyDataMapper MyReadPolyData_Color_by_Mapper_And_Lut.cpp)

target_compile_features(VtkReader PUBLIC cxx_std_20)
target_compile_definitions(VtkReader PUBLIC BUILD_API)

# include directory
target_include_directories(VtkReader PUBLIC
  ${VTK_INCLUDE}
)

# source tree
list(APPEND private_header_list
)
list(APPEND target_source_list
  MyReadPolyData.cpp
  #ReadAllPolyDataTypes.cxx
)

source_group("Header Files" FILES ${private_header_list})
source_group("Source Files" FILES ${target_source_list})

target_sources(VtkReader
   PRIVATE
   ${target_source_list}
  
  PRIVATE
  FILE_SET private_headers
  TYPE HEADERS
  BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/VtkReader"
  FILES ${private_header_list}
)

# library
target_link_directories(VtkReader PUBLIC "${VTK_LIBS}")
target_link_libraries(VtkReader PRIVATE ${VTK_LIBRARIES})

target_link_directories(MyReadPolyDataMapper PUBLIC "${VTK_LIBS}")
target_link_libraries(MyReadPolyDataMapper PRIVATE ${VTK_LIBRARIES})

