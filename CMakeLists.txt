cmake_minimum_required(VERSION 3.18)

project(
  SpeakEasy2
  DESCRIPTION "igraph implementation of SpeakEasy2 community detection."
  LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(SE2_PARALLEL "Process independent runs in parallel" ON)

find_package(Threads)
if(SE2_PARALLEL AND NOT Threads_FOUND)
  set(SE2_PARALLEL OFF)
  message(WARNING "Threads not found. Compiling without parallel support")
endif()

add_subdirectory(vendor)

if("${CMAKE_PACKAGE_VERSION}" STREQUAL "")
  if(NOT GIT_FOUND)
    find_package(Git QUIET)
  endif()

  if(GIT_FOUND)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} describe --tags
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      OUTPUT_VARIABLE CMAKE_PACKAGE_VERSION
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
endif()

if("${CMAKE_PACKAGE_VERSION}" STREQUAL "")
  set(CMAKE_PACKAGE_VERSION "NOTFOUND")
endif()

message(STATUS "${PROJECT_NAME} version: ${CMAKE_PACKAGE_VERSION}")
add_subdirectory(src/speakeasy2)
target_compile_options(SpeakEasy2 PRIVATE -Wall -pedantic)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Sanitizer")
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g -O0)
  add_link_options(-fsanitize=address -fno-omit-frame-pointer)
endif()

get_directory_property(definitions COMPILE_DEFINITIONS)
if(NOT "${definitions}" MATCHES "USING_R")
  add_subdirectory(examples/)
endif()
