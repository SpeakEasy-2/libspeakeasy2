cmake_minimum_required(VERSION 3.18)

project(
  SpeakEasy2
  DESCRIPTION "igraph implementation of SpeakEasy2 community detection."
  LANGUAGES C CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS OFF)

add_subdirectory(vendor/igraph)

if ("${CMAKE_PACKAGE_VERSION}" STREQUAL "")
  if (NOT GIT_FOUND)
    find_package(Git QUIET)
  endif()

  execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE CMAKE_PACKAGE_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
endif()

if ("${CMAKE_PACKAGE_VERSION}" STREQUAL "")
  set(CMAKE_PACKAGE_VERSION "NOTFOUND")
endif()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/se2_version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/se2_version.h
)

message(STATUS "${PROJECT_NAME} version: ${CMAKE_PACKAGE_VERSION}")
add_subdirectory(src/speakeasy2)

get_directory_property(definitions COMPILE_DEFINITIONS)
if (NOT "${definitions}" MATCHES "USING_R")
  add_subdirectory(examples/)
endif()
