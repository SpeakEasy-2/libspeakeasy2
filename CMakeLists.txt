cmake_minimum_required(VERSION 3.18)

project(
  igraph-SE2
  DESCRIPTION "igraph implementation of SpeakEasy2 community detection."
  LANGUAGES C CXX
)

find_package(Matlab COMPONENTS MAIN_PROGRAM MAT_LIBRARY MX_LIBRARY MEX_COMPILER REQUIRED)

message(STATUS "Matlab: ${Matlab_VERSION_STRING}")

if(Matlab_VERSION_STRING VERSION_LESS 9.14)
  message(WARNING "Matlab >= 9.14 (R2023a) is required for MEX tests to work due to new buildtool argument syntax.")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS OFF)

set(IGRAPH_USE_INTERNAL_BLAS ON)
set(IGRAPH_USE_INTERNAL_LAPACK ON)
set(IGRAPH_USE_INTERNAL_ARPACK ON)
set(IGRAPH_USE_INTERNAL_GMP ON)
set(IGRAPH_USE_INTERNAL_GLPK ON)
set(IGRAPH_USE_INTERNAL_plfit ON)
add_subdirectory(vendor/igraph)

include(vendor/igraph/etc/cmake/GetGitRevisionDescription.cmake)
if ("${CMAKE_PACKAGE_VERSION}" STREQUAL "")
    git_describe(CMAKE_PACKAGE_VERSION)
endif()

if ("${CMAKE_PACKAGE_VERSION}" STREQUAL "")
  set(CMAKE_PACKAGE_VERSION "NOTFOUND")
endif()

# Get name Matlab uses for the different architectures/systems.
if ("${Matlab_MEX_EXTENSION}" STREQUAL "mexa64")
  set(Matlab_ARCH "glnxa64")
elseif ("${Matlab_MEX_EXTENSION}" STREQUAL "mexarm64")
  set(Matlab_ARCH "linux-arm-64")
elseif ("${Matlab_MEX_EXTENSION}" STREQUAL "mexmaca64")
  set(Matlab_ARCH "maca64")
elseif ("${Matlab_MEX_EXTENSION}" STREQUAL "mexmaci64")
  set(Matlab_ARCH "maci64")
elseif ("${Matlab_MEX_EXTENSION}" STREQUAL "mexw32")
  set(Matlab_ARCH "win32")
elseif ("${Matlab_MEX_EXTENSION}" STREQUAL "mexw64")
  set(Matlab_ARCH "win64")
else ()
  set(Matlab_ARCH "NOTFOUND")
endif()

message(STATUS "${CMAKE_PROJECT_NAME} version: ${CMAKE_PACKAGE_VERSION}")
message(STATUS "MEX extension: ${Matlab_MEX_EXTENSION}")
message(STATUS "Matlab arch: ${Matlab_ARCH}")
message(STATUS "")

add_subdirectory(vendor/matlab-igraph/mxIgraph)
add_subdirectory(src/speakeasy2)
add_subdirectory(src/mex)

# Installation
## MATLAB Toolbox
install(DIRECTORY toolbox
  DESTINATION igraph-SE2
  COMPONENT toolbox)

install(TARGETS mexSE2 isdirected
  DESTINATION igraph-SE2/toolbox/private
  RUNTIME_DEPENDENCY_SET toolbox_deps
  COMPONENT toolbox)

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  install(RUNTIME_DEPENDENCY_SET toolbox_deps
    DIRECTORIES
    "C:\\msys64\\mingw64\\bin" "C:\\msys64\\ucrt64\\bin"
    "D:\\msys64\\mingw64\\bin" "D:\\msys64\\ucrt64\\bin"
    PRE_EXCLUDE_REGEXES "api-ms-*" "libmx" "libmex" "ext-ms-*"
    DESTINATION igraph-SE2/toolbox/private
    COMPONENT toolbox)
endif()

# Packaging
set(CPACK_PACKAGE_HOMEPAGE_URL("https://github.com/DavidRConnell/igraph-SE2"))

set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${CMAKE_PACKAGE_VERSION}_source")
set(CPACK_PACKAGE_FILE_NAME
  "${CMAKE_PROJECT_NAME}_${CMAKE_PACKAGE_VERSION}_${Matlab_ARCH}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_GENERATOR "TGZ")
if(CMAKE_SYSTEM MATCHES Windows)
  set(CPACK_GENERATOR "ZIP")
endif()

set(CPACK_SOURCE_IGNORE_FILES
  "\\\\..*/"
  "\\\\.${Matlab_MEX_EXTENSION}$"
  "${CMAKE_SOURCE_DIR}/build"
  "${CMAKE_SOURCE_DIR}/dist"
  "compile_commands.json")

# Only package files needed to run the toolbox.
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL "Unspecified")
include(CPack)