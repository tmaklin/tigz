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
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ffunction-sections -fdata-sections -Wl,-gc-sections -s -Wl,--strip-all -fmerge-all-constants -Wl,-z,norelro -fPIE -fPIC -DNDEBUG -fno-exceptions")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffunction-sections -fdata-sections -Wl,-gc-sections -s -Wl,--strip-all -fmerge-all-constants -Wl,-z,norelro -fPIE -fPIC")

## Rapidgzip doesn't compile without this
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconstexpr-ops-limit=67108864")

if (CMAKE_WITH_NATIVE_INSTRUCTIONS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -mtune=native")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native")
else()
  set(CMAKE_WITH_NATIVE_INSTRUCTIONS 0)
endif()

if(CMAKE_PGO_GENERATE AND NOT CMAKE_PGO_USE)
  if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
  else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-generate")
  endif()
  if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-generate")
  endif()
endif()

if(CMAKE_PGO_USE)
  if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-instr-use=${CMAKE_CURRENT_BINARY_DIR}/default.profdata -fprofile-correction")
  else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-use -fprofile-correction")
  endif()
  if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-use=${CMAKE_CURRENT_BINARY_DIR}/default.profdata -fprofile-correction")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-use -fprofile-correction")
  endif()
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

## Threads
find_package(Threads REQUIRED)
target_link_libraries(tigz Threads::Threads)

### libdeflate
if (DEFINED CMAKE_LIBDEFLATE_HEADERS AND DEFINED CMAKE_LIBDEFLATE_LIBRARY)
  message(STATUS "libdeflate headers provided in: ${CMAKE_LIBDEFLATE_HEADERS}")
  message(STATUS "libdeflate library provided in: ${CMAKE_LIBDEFLATE_LIBRARY}")
else()
ExternalProject_Add(libdeflate
  GIT_REPOSITORY    https://github.com/ebiggers/libdeflate
  GIT_TAG           v1.18
  PREFIX            "external"
  BINARY_DIR        "${CMAKE_CURRENT_BINARY_DIR}/external/libdeflate"
  BUILD_IN_SOURCE   0
  CMAKE_ARGS	    -D LIBDEFLATE_BUILD_SHARED_LIB=OFF
  		    -D LIBDEFLATE_BUILD_STATIC_LIB=ON
  		    -D LIBDEFLATE_BUILD_GZIP=OFF
		    -D LIBDEFLATE_BUILD_TESTS=OFF
		    -D LIBDEFLATE_DECOMPRESSION_SUPPORT=OFF
		    -D LIBDEFLATE_ZLIB_SUPPORT=OFF
		    -D LIBDEFLATE_GZIP_SUPPORT=ON
		    -D LIBDEFLATE_DECOMPRESSION_SUPPORT=OFF
		    -D "CMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
		    -D "CMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
  INSTALL_COMMAND   cp ${CMAKE_CURRENT_BINARY_DIR}/external/src/libdeflate/libdeflate.h ${CMAKE_CURRENT_BINARY_DIR}/external/libdeflate/
  )
  add_dependencies(tigz libdeflate)
  set(CMAKE_LIBDEFLATE_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/external/libdeflate)
  set(CMAKE_LIBDEFLATE_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/external/libdeflate/libdeflate.a)
endif()
include_directories(${CMAKE_LIBDEFLATE_HEADERS})
target_link_libraries(tigz ${CMAKE_LIBDEFLATE_LIBRARY})

### zlib-ng
if (DEFINED CMAKE_ZLIB_HEADERS AND DEFINED CMAKE_ZLIB_LIBRARY)
  message(STATUS "zlib headers provided in: ${CMAKE_ZLIB_HEADERS}")
  message(STATUS "zlib library provided in: ${CMAKE_ZLIB_LIBRARY}")
else()
  ExternalProject_Add(zlib-ng
    GIT_REPOSITORY    https://github.com/zlib-ng/zlib-ng
    GIT_TAG           2.1.3
    PREFIX            "external"
    BINARY_DIR        "${CMAKE_CURRENT_BINARY_DIR}/external/zlib-ng"
    BUILD_IN_SOURCE   0
    CMAKE_ARGS      -D ZLIB_COMPAT=ON
		    -D ZLIB_ENABLE_TESTS=OFF
		    -D ZLIBNG_ENABLE_TESTS=OFF
		    -D WITH_GTEST=OFF
		    -D BUILD_SHARED_LIBS=0
		    -D WITH_NATIVE_INSTRUCTIONS=${CMAKE_WITH_NATIVE_INSTRUCTIONS}
		    -D WITH_OPTIM=ON
		    -D WITH_NEW_STRATEGIES=ON
		    -D "CMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
		    -D "CMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    INSTALL_COMMAND   ""
    )
  add_dependencies(tigz zlib-ng)
  set(CMAKE_ZLIB_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/external/zlib-ng)
  set(CMAKE_ZLIB_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/external/zlib-ng/libz.a)
endif()
include_directories(${CMAKE_ZLIB_HEADERS})
target_link_libraries(tigz ${CMAKE_ZLIB_LIBRARY})

### cxxopts
if (DEFINED CMAKE_CXXOPTS_HEADERS)
  message(STATUS "cxxopts headers provided in: ${CMAKE_CXXOPTS_HEADERS}")
else()
  ExternalProject_Add(cxxopts
    GIT_REPOSITORY    https://github.com/jarro2783/cxxopts
    GIT_TAG           v3.1.1
    PREFIX            "external"
    SOURCE_DIR        "${CMAKE_CURRENT_BINARY_DIR}/external/cxxopts"
    BUILD_IN_SOURCE   1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
    INSTALL_COMMAND   ""
    )
  add_dependencies(tigz cxxopts)
  set(CMAKE_CXXOPTS_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/external/cxxopts/include)
endif()
include_directories(${CMAKE_CXXOPTS_HEADERS})

### rapidgzip
if (DEFINED CMAKE_RAPIDGZIP_HEADERS)
  message(STATUS "rapidgzip headers provided in: ${CMAKE_RAPIDGZIP_HEADERS}")
else()
  ExternalProject_Add(rapidgzip
    GIT_REPOSITORY		https://github.com/mxmlnkn/rapidgzip
    GIT_TAG			rapidgzip-v0.10.1
    GIT_SUBMODULES		""
    GIT_SUBMODULES_RECURSE	0
    PREFIX            "external"
    SOURCE_DIR			"${CMAKE_CURRENT_BINARY_DIR}/external/rapidgzip"
    BUILD_IN_SOURCE   		1
    BUILD_COMMAND	    	""
    CONFIGURE_COMMAND 		""
    INSTALL_COMMAND   		""
    )
  add_dependencies(tigz rapidgzip)
  set(CMAKE_RAPIDGZIP_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/external/rapidgzip/src/core ${CMAKE_CURRENT_BINARY_DIR}/external/rapidgzip/src/rapidgzip)
endif()
include_directories(${CMAKE_RAPIDGZIP_HEADERS})

### BS::thread_pool
if (DEFINED CMAKE_BS_THREAD_POOL_HEADERS)
  message(STATUS "BS::thread_pool headers provided in: ${CMAKE_BS_THREAD_POOL_HEADERS}")
else()
  ExternalProject_Add(bs_thread_pool
    GIT_REPOSITORY    https://github.com/bshoshany/thread-pool
    GIT_TAG           v3.5.0
    PREFIX            "external"
    SOURCE_DIR        "${CMAKE_CURRENT_BINARY_DIR}/external/bs_thread_pool"
    BUILD_IN_SOURCE   1
    BUILD_COMMAND     ""
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND   ""
    )
  add_dependencies(tigz bs_thread_pool)
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

## make install
install(TARGETS tigz)
