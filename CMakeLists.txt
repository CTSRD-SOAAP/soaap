cmake_minimum_required(VERSION 2.8)

# Allow user to optionally override LLVM build tree locations.
set(LLVM_TOOLS_BINARY_DIR "" CACHE PATH "Path to llvm/bin")
set(LLVM_LIBRARY_DIR "" CACHE PATH "Path to llvm/lib")
set(LLVM_MAIN_INCLUDE_DIR "" CACHE PATH "Path to llvm/include")
set(LLVM_BINARY_DIR "" CACHE PATH "Path to LLVM build tree")
set(LLVM_MAIN_SRC_DIR "" CACHE PATH "Path to LLVM source tree")

find_package(LLVM REQUIRED NO_CMAKE_PATH)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

foreach(v
    TOOLS_BINARY_DIR
    LIBRARY_DIR
    MAIN_INCLUDE_DIR
    BINARY_DIR
    MAIN_SRC_DIR
    )
  if(LLVM_BUILD_${v} AND NOT LLVM_${v})
    set(LLVM_${v} "${LLVM_BUILD_${v}}")
  endif()
endforeach()

include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fno-rtti -std=c++11")

# Default to libc++ if installed
find_package(LibCXX)
if (LIBCXX_FOUND)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  include_directories(${LIBCXX_INCLUDE_DIRS})
  link_directories(${LIBCXX_LIBRARIES})
endif()

# these need to be set for set_output_directory() in add_llvm_loadable_module()
set(LLVM_RUNTIME_OUTPUT_INTDIR ${PROJECT_BINARY_DIR}/bin)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${PROJECT_BINARY_DIR}/lib)

set(LIT ${LLVM_BINARY_DIR}/bin/llvm-lit CACHE FILE "Location of llvm-lit")

include_directories(${LLVM_MAIN_INCLUDE_DIR})
include_directories(${LLVM_MAIN_SRC_DIR}/lib)
include_directories(${LLVM_MAIN_SRC_DIR}/runtime/libprofile)
include_directories(${LLVM_BINARY_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/soaap)
link_directories(${LLVM_LIBRARY_DIR})

add_subdirectory(include)
add_subdirectory(soaap)
add_subdirectory(tests)
add_subdirectory(tools)
