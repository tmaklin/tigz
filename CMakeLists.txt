cmake_minimum_required (VERSION 3.16)
project(tigz)
include(ExternalProject)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

## Determine build type and set flags accordingly
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release) ## Build Release by default
endif()
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Wpedantic -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

## Rapidgzip doesn't compile without this
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconstexpr-ops-limit=67108864")

if (CMAKE_WITH_NATIVE_INSTRUCTIONS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -mtune=native")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native")
else()
  set(CMAKE_WITH_NATIVE_INSTRUCTIONS 0)
endif()

## Link time optimization
if(CMAKE_WITH_FLTO)
  cmake_policy(SET CMP0069 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION 1)
endif()

cmake_policy(SET CMP0097 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0097 NEW)

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

## Dependencies

### libdeflate
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
  ExternalProject_Add(libdeflate
    SOURCE_DIR        "${CMAKE_CURRENT_BINARY_DIR}/external/libdeflate"
    BUILD_IN_SOURCE   0
    CMAKE_ARGS	      -D LIBDEFLATE_BUILD_SHARED_LIB=OFF
                      -D LIBDEFLATE_BUILD_STATIC_LIB=ON
		      -D LIBDEFLATE_BUILD_GZIP=OFF
		      -D LIBDEFLATE_BUILD_TESTS=OFF
		      -D LIBDEFLATE_DECOMPRESSION_SUPPORT=OFF
		      -D LIBDEFLATE_ZLIB_SUPPORT=OFF
    INSTALL_COMMAND    ""
    TEST_COMMAND       ""
    UPDATE_COMMAND     ""
    )
  add_dependencies(tigz libdeflate)
  set(CMAKE_LIBDEFLATE_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/external/libdeflate)
  set(CMAKE_LIBDEFLATE_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/libdeflate-prefix/src/libdeflate-build/libdeflate.a)
endif()
include_directories(${CMAKE_LIBDEFLATE_HEADERS})
target_link_libraries(tigz ${CMAKE_LIBDEFLATE_LIBRARY})

### zlib-ng
if (DEFINED CMAKE_ZLIB_HEADERS AND DEFINED CMAKE_ZLIB_LIBRARY)
  message(STATUS "zlib headers provided in: ${CMAKE_ZLIB_HEADERS}")
  message(STATUS "zlib library provided in: ${CMAKE_ZLIB_LIBRARY}")
else()
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/CMakeLists-zlibng.txt.in ${CMAKE_BINARY_DIR}/external/zlib-ng-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/zlib-ng-download )
  if(result)
    message(FATAL_ERROR "CMake step for zlib-ng failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/zlib-ng-download )
  if(result)
    message(FATAL_ERROR "Build step for zlib-ng failed: ${result}")
  endif()
  ExternalProject_Add(zlibng
    SOURCE_DIR        "${CMAKE_CURRENT_BINARY_DIR}/external/zlib-ng"
    BUILD_IN_SOURCE   0
    CMAKE_ARGS	    -D ZLIB_COMPAT=ON
  		    -D ZLIB_ENABLE_TESTS=OFF
		    -D ZLIBNG_ENABLE_TESTS=OFF
		    -D WITH_GTEST=OFF
		    -D BUILD_SHARED_LIBS=0
		    -D WITH_NATIVE_INSTRUCTIONS=CMAKE_WITH_NATIVE_INSTRUCTIONS
    INSTALL_COMMAND   ""
    TEST_COMMAND      ""
    UPDATE_COMMAND    "")
  add_dependencies(tigz zlibng)
  set(CMAKE_ZLIB_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/zlibng-prefix/src/zlibng-build)
  set(CMAKE_ZLIB_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/zlibng-prefix/src/zlibng-build/libz.a)
endif()
include_directories(${CMAKE_ZLIB_HEADERS})
target_link_libraries(tigz ${CMAKE_ZLIB_LIBRARY})

### cxxopts
if (DEFINED CMAKE_CXXOPTS_HEADERS)
  message(STATUS "cxxopts headers provided in: ${CMAKE_CXXOPTS_HEADERS}")
else()
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/CMakeLists-cxxopts.txt.in ${CMAKE_BINARY_DIR}/external/cxxopts-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/cxxopts-download )
  if(result)
    message(FATAL_ERROR "CMake step for cxxopts failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/cxxopts-download )
  if(result)
    message(FATAL_ERROR "Build step for cxxopts failed: ${result}")
  endif()
  set(CMAKE_CXXOPTS_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/external/cxxopts/include)
endif()
include_directories(${CMAKE_CXXOPTS_HEADERS})

### rapidgzip
if (DEFINED CMAKE_RAPIDGZIP_HEADERS)
  message(STATUS "rapidgzip headers provided in: ${CMAKE_RAPIDGZIP_HEADERS}")
else()
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/CMakeLists-rapidgzip.txt.in ${CMAKE_BINARY_DIR}/external/rapidgzip-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/rapidgzip-download )
  if(result)
    message(FATAL_ERROR "CMake step for rapidgzip failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/rapidgzip-download )
  if(result)
    message(FATAL_ERROR "Build step for rapidgzip failed: ${result}")
  endif()
  set(CMAKE_RAPIDGZIP_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/external/rapidgzip/src/core ${CMAKE_CURRENT_BINARY_DIR}/external/rapidgzip/src/rapidgzip)
endif()
include_directories(${CMAKE_RAPIDGZIP_HEADERS})

### BS::thread_pool
if (DEFINED CMAKE_BS_THREAD_POOL_HEADERS)
  message(STATUS "BS::thread_pool headers provided in: ${CMAKE_BS_THREAD_POOL_HEADERS}")
else()
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/CMakeLists-bs_thread_pool.txt.in ${CMAKE_BINARY_DIR}/external/bs_thread_pool-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/bs_thread_pool-download )
  if(result)
    message(FATAL_ERROR "CMake step for BS::thread_pool failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/bs_thread_pool-download )
  if(result)
    message(FATAL_ERROR "Build step for BS::thread_pool failed: ${result}")
  endif()
  set(CMAKE_BS_THREAD_POOL_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/external/bs_thread_pool/include)
endif()
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
