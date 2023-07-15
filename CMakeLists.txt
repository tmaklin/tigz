cmake_minimum_required (VERSION 3.0)
project(tigz)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

## Determine build type and set flags accordingly
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release) ## Build Release by default
endif()
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Wpedantic -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

## Link time optimization
if(CMAKE_BUILD_WITH_FLTO)
  cmake_policy(SET CMP0069 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION 1)
endif()

## Set C++17 support
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

## alignment-writer executable
add_executable(tigz ${CMAKE_CURRENT_SOURCE_DIR}/src/tigz.cpp)

## Include project headers
if (NOT DEFINED CMAKE_TIGZ_HEADERS)
  set(CMAKE_TIGZ_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/include)
endif()
include_directories(${CMAKE_TIGZ_HEADERS})

## libdeflate
if (DEFINED CMAKE_LIBDEFLATE_HEADERS AND DEFINED CMAKE_LIBDEFLATE_LIBRARY)
  message(STATUS "libdeflate headers provided in: ${CMAKE_LIBDEFLATE_HEADERS}")
  message(STATUS "libdeflate library provided in: ${CMAKE_LIBDEFLATE_LIBRARY}")
else()
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/CMakeLists-libdeflate.txt.in ${CMAKE_BINARY_DIR}/external/libdeflate-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/libdeflate-download )
  if(result)
    message(FATAL_ERROR "CMake step for libdeflate failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/libdeflate-download )
  if(result)
    message(FATAL_ERROR "Build step for libdeflate failed: ${result}")
  endif()
  set(CMAKE_LIBDEFLATE_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/external/libdeflate/include)
  set(CMAKE_LIBDEFLATE_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/external/libdeflate/build)
endif()
include_directories(${CMAKE_LIBDEFLATE_HEADERS})
target_link_libraries(tigz deflate)

## cxxargs
if (DEFINED CMAKE_CXXARGS_HEADERS)
  message(STATUS "cxxargs headers provided in: ${CMAKE_CXXARGS_HEADERS}")
else()
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/CMakeLists-cxxargs.txt.in ${CMAKE_BINARY_DIR}/external/cxxargs-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/cxxargs-download )
  if(result)
    message(FATAL_ERROR "CMake step for cxxargs failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/cxxargs-download )
  if(result)
    message(FATAL_ERROR "Build step for cxxargs failed: ${result}")
  endif()
  set(CMAKE_CXXARGS_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/external/cxxargs/include)
endif()
include_directories(${CMAKE_CXXARGS_HEADERS})

set(CMAKE_BS_THREAD_POOL_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/external/thread-pool/include)
include_directories(${CMAKE_BS_THREAD_POOL_HEADERS})

## Get version number from git tags
find_package(Git)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE TIGZ_BUILD_VERSION
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message( STATUS "tigz version: ${TIGZ_BUILD_VERSION}")
else()
  message(STATUS "GIT not found, version numbering disabled")
  set(TIGZ_BUILD_VERSION "unknown")
endif()
string(TIMESTAMP TIGZ_BUILD_TIMESTAMP)

## Generate a version.h file containing build version and timestamp
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/tigz_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/tigz_version.h @ONLY)
