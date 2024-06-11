file(GLOB src_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
add_library(SpeakEasy2
  ${src_files}
)

target_link_libraries(SpeakEasy2 PUBLIC igraph)

if (${IGRAPH_THREAD_SAFE})
  find_package(OpenMP)
  if (OpenMP_C_FOUND)
    message(STATUS "Found OpenMP")
    target_link_libraries(SpeakEasy2 PUBLIC OpenMP::OpenMP_C)
  else ()
    message(STATUS "OpenMP not found. Compiling without OMP support.")
  endif ()
else ()
  message(STATUS "Not compiled thread safe, compiling without OMP support.")
endif ()

target_include_directories(SpeakEasy2
  PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/include
)
