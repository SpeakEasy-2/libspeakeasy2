cmake_minimum_required(VERSION 3.18)

project(
  SpeakEasy2
  DESCRIPTION "igraph implementation of SpeakEasy2 community detection."
  LANGUAGES C)

set(CMAKE_C_STANDARD 11) # Needed for threads.h->thrd_sleep on MSVC.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(SE2_PARALLEL "Process independent runs in parallel" ON)

if(SE2_PARALLEL AND MSVC)
  find_package(PThreads4W)
  set(PThreads_FOUND PThreads4W_FOUND)
  if(PThreads_FOUND)
    set(PThreads_LIBRARY PThreads4W::PThreads4W)
  endif()
elseif(SE2_PARALLEL)
  find_package(Threads)
  set(PThreads_FOUND "${CMAKE_USE_PTHREADS_INIT}")
  if(PThreads_FOUND)
    set(PThreads_LIBRARY Threads::Threads)
  endif()
endif()

if(SE2_PARALLEL AND NOT PThreads_FOUND)
  set(SE2_PARALLEL OFF)
  message(
    WARNING "No pthreads library found. Compiling without parallel support")
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

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/se2_version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/se2_version.h)

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "${PROJECT_NAME} version: ${CMAKE_PACKAGE_VERSION}")
add_subdirectory(src/speakeasy2)
target_compile_options(
  SpeakEasy2
  PRIVATE -Wall
          -pedantic
          $<$<CONFIG:Release>:-O3
          -ffast-math>
          $<$<CONFIG:Sanitizer>:-fsanitize=address
          -fno-omit-frame-pointer
          -g
          -O0>
          $<$<CONFIG:Profile>:-pg>)
target_link_options(SpeakEasy2 PRIVATE $<$<CONFIG:Sanitizer>:-fsanitize=address
                    -fno-omit-frame-pointer> $<$<CONFIG:Profile>:-pg>)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Sanitizer")
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g -O0)
  add_link_options(-fsanitize=address -fno-omit-frame-pointer)
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Profile")
  set(SE2_PARALLEL OFF)
  message(WARNING "Turning off parallel support for profiling.")
  add_compile_options(-pg)
  add_link_options(-pg)
endif()

if(PROJECT_IS_TOP_LEVEL)
  add_subdirectory(examples/)
endif()
