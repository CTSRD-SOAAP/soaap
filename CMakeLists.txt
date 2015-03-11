cmake_minimum_required(VERSION 2.8)

if (NOT CMAKE_VERSION VERSION_LESS 2.8.12)
  if (POLICY CMP0022)
    cmake_policy(SET CMP0022 NEW)
  endif()
endif()

# Allow user to optionally override LLVM build tree locations.
set(LLVM_TOOLS_BINARY_DIR "" CACHE PATH "Path to llvm/bin")
set(LLVM_LIBRARY_DIR "" CACHE PATH "Path to llvm/lib")
set(LLVM_MAIN_INCLUDE_DIR "" CACHE PATH "Path to llvm/include")
set(LLVM_BINARY_DIR "" CACHE PATH "Path to LLVM build tree")
set(LLVM_MAIN_SRC_DIR "" CACHE PATH "Path to LLVM source tree")

find_package(LLVM REQUIRED NO_CMAKE_PATH) # config mode search
message(STATUS "Found LLVM: ${LLVM_DIR}")

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

include(FeatureSummary)

list(APPEND CMAKE_MODULE_PATH "${LLVM_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_C_COMPILER ${LLVM_BINARY_DIR}/bin/clang CACHE FILE "C compiler" FORCE)
set(CMAKE_CXX_COMPILER ${LLVM_BINARY_DIR}/bin/clang++ CACHE FILE "C++ compiler" FORCE)
message(STATUS "Setting C compiler to ${CMAKE_C_COMPILER}")
message(STATUS "Setting C++ compiler to ${CMAKE_CXX_COMPILER}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fno-rtti -std=c++11")

# libxo
include(ExternalProject)
ExternalProject_Add(libxo
  URL ${CMAKE_CURRENT_SOURCE_DIR}/libxo
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND rm -rf configure m4
                    COMMAND sh bin/setup.sh
                    COMMAND cd build
                    COMMAND ../configure --prefix=<INSTALL_DIR>
  BUILD_COMMAND cd <SOURCE_DIR>/build COMMAND gmake 
  INSTALL_COMMAND cd <SOURCE_DIR>/build COMMAND gmake install
  STEP_TARGETS build install
)

ExternalProject_Get_Property(libxo install_dir)
include_directories(${install_dir}/include)
link_directories(${install_dir}/lib)

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

feature_summary(FATAL_ON_MISSING_REQUIRED_PACKAGES WHAT ALL)
