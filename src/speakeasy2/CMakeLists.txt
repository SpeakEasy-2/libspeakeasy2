file(GLOB src_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
add_library(SpeakEasy2
  OBJECT
  ${src_files}
)

find_package(OpenMP)

target_link_libraries(SpeakEasy2
  PUBLIC igraph OpenMP::OpenMP_C
)

target_include_directories(SpeakEasy2
  PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/include
)
