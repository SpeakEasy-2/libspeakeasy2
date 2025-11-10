file(GLOB src_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
add_library(SpeakEasy2 ${src_files})

target_link_libraries(SpeakEasy2 PUBLIC igraph)

if(SE2_PARALLEL)
  target_link_libraries(SpeakEasy2 PUBLIC "${PThreads_LIBRARY}")
endif()

target_include_directories(SpeakEasy2 PUBLIC ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_BINARY_DIR}/include)
