cmake_minimum_required(VERSION 2.8)

if (NOT CMAKE_VERSION VERSION_LESS 2.8.12)
  if (POLICY CMP0022)
    cmake_policy(SET CMP0022 NEW)
  endif()
endif()

# Allow user to optionally override LLVM build tree locations.
set(LLVM_BUILD_DIR "" CACHE PATH "Path to LLVM build directory")
set(LLVM_DIR "" CACHE PATH "Path to LLVM cmake config files")
set(LLVM_TOOLS_BINARY_DIR "" CACHE PATH "Path to llvm/bin")
set(LLVM_LIBRARY_DIR "" CACHE PATH "Path to llvm/lib")
set(LLVM_MAIN_INCLUDE_DIR "" CACHE PATH "Path to llvm/include")
set(LLVM_BINARY_DIR "" CACHE PATH "Path to LLVM build tree")
set(LLVM_MAIN_SRC_DIR "" CACHE PATH "Path to LLVM source tree")

if(NOT LLVM_BUILD_DIR)
  # compatibility with the previous build instructions
  if(LLVM_DIR)
    set(LLVM_BUILD_DIR "${LLVM_DIR}/../../.." CACHE PATH "Path to LLVM build directory" FORCE)
    message(STATUS "Inferring LLVM_BUILD_DIR from LLVM_DIR")
  else()
    # If ${CMAKE_C_COMPILER} points to <llvm build dir>/bin/clang we can just infer LLVM_BUILD_DIR from that
    # This handles the case where CMake reruns (with empty cache!) because the CMAKE_C_COMPILER was changed
    get_filename_component(COMPILER_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
    if(EXISTS "${COMPILER_DIR}/../LLVMBuild.cmake")
      message(STATUS "Inferring LLVM_BUILD_DIR from CMAKE_C_COMPILER")
      set(LLVM_BUILD_DIR "${COMPILER_DIR}/..")
    else()
      message(STATUS "${COMPILER_DIR}/../LLVMBuild.cmake does not exist")
      message(FATAL_ERROR "LLVM_BUILD_DIR not defined! It must point to the directory where LLVM was built. Run CMake -DLLVM_BUILD_DIR=<dir> to fix this.")
    endif()
  endif()
endif()

get_filename_component(LLVM_BUILD_DIR "${LLVM_BUILD_DIR}" REALPATH)
message(STATUS "LLVM_BUILD_DIR: ${LLVM_BUILD_DIR}, LLVM_DIR: ${LLVM_DIR}")

# only search in ${LLVM_BUILD_DIR} and not in any of the default search locations:
# this is done by passing PATHS ${LLVM_DIR} and NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH
# message("Looking for LLVM in ${LLVM_BUILD_DIR}")
find_package(LLVM REQUIRED CONFIG PATHS ${LLVM_BUILD_DIR} NO_DEFAULT_PATH)
message(STATUS "Found LLVM: ${LLVM_BUILD_BINARY_DIR}")

foreach(v
    TOOLS_BINARY_DIR
    LIBRARY_DIR
    MAIN_INCLUDE_DIR
    BINARY_DIR
    MAIN_SRC_DIR
    )
  if (NOT LLVM_${v})
    set(LLVM_${v} ${LLVM_BUILD_${v}} CACHE PATH "Path to LLVM ${v}" FORCE)
  endif()
  if(NOT LLVM_${v})
    # these variables must be set in LLVMConfig.cmake
    message(FATAL_ERROR "Required variable LLVM_BUILD_${v} is not set!")
  endif()
endforeach()

include(GNUInstallDirs)
include(FeatureSummary)

list(APPEND CMAKE_MODULE_PATH "${LLVM_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})

# LLVM_BINARY_DIR must be set otherwise there would have been an error earlier
if(NOT ${CMAKE_C_COMPILER} STREQUAL "${LLVM_TOOLS_BINARY_DIR}/clang")
  message(STATUS "${CMAKE_C_COMPILER} no longer matches ${LLVM_TOOLS_BINARY_DIR}/clang")
  set(CMAKE_C_COMPILER ${LLVM_TOOLS_BINARY_DIR}/clang CACHE STRING "C compiler" FORCE)
  set(CMAKE_CXX_COMPILER ${LLVM_TOOLS_BINARY_DIR}/clang++ CACHE STRING "C++ compiler" FORCE)
  message(STATUS "Setting C compiler to ${CMAKE_C_COMPILER}")
  message(STATUS "Setting C++ compiler to ${CMAKE_CXX_COMPILER}")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fno-rtti -std=c++11")

# libxo
set(LIBXO_NEED_VERSION "0.2.0")

include(FindLibXO)
if ("${LIBXO_VERSION}" STREQUAL ${LIBXO_NEED_VERSION})
  message(STATUS "Found libxo: ${LIBXO_PREFIX}")
else()
  include(ExternalLibXO)
  message(STATUS "Building libxo: ${LIBXO_PREFIX}")
endif ()

include_directories(${LIBXO_INCLUDE_DIRS})

# Default to libc++ if installed
find_package(LibCXX)
set_package_properties(LibCXX PROPERTIES TYPE OPTIONAL URL "http://libcxx.llvm.org/"
                              PURPOSE "Use LLVM libc++ instead of GNU libstdc++")
if (LIBCXX_FOUND)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  include_directories(${LIBCXX_INCLUDE_DIRS})
  foreach(lib ${LIBCXX_LIBRARIES})
    GET_FILENAME_COMPONENT(LIBCXX_LIBRARY_DIR ${lib} DIRECTORY)
    link_directories(${LIBCXX_LIBRARY_DIR})
  endforeach(lib)
endif()

# these need to be set for set_output_directory() in add_llvm_loadable_module()
set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib)

set(LIT ${LLVM_BUILD_BINARY_DIR}/bin/llvm-lit CACHE FILE "Location of llvm-lit")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${LLVM_MAIN_SRC_DIR}/lib)
include_directories(${LLVM_MAIN_SRC_DIR}/runtime/libprofile)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/soaap)
link_directories(${LLVM_LIBRARY_DIR}) # required so that RPATH is correct

add_subdirectory(include)
add_subdirectory(soaap)
add_subdirectory(tests)
add_subdirectory(tools)

feature_summary(FATAL_ON_MISSING_REQUIRED_PACKAGES WHAT ALL)
