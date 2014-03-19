cmake_minimum_required(VERSION 2.8)

if(NOT CMAKE_LLVM_CONFIG)
  set(CMAKE_LLVM_CONFIG llvm-config)
endif()

# Make sure we can find llvm-config!
exec_program(${CMAKE_LLVM_CONFIG} ARGS --version  OUTPUT_VARIABLE LLVM_VERSION)                                 
if("${LLVM_VERSION}" MATCHES "not found")
  message(FATAL_ERROR "llvm-config not found; may need to set PATH?")
endif()

exec_program(${CMAKE_LLVM_CONFIG} ARGS --includedir OUTPUT_VARIABLE LLVM_INCDIR)
exec_program(${CMAKE_LLVM_CONFIG} ARGS --libdir   OUTPUT_VARIABLE LLVM_LIBDIR)
exec_program(${CMAKE_LLVM_CONFIG} ARGS --src-root OUTPUT_VARIABLE LLVM_SRCDIR)
exec_program(${CMAKE_LLVM_CONFIG} ARGS --obj-root OUTPUT_VARIABLE LLVM_OBJDIR)

set(CMAKE_MODULE_PATH "${LLVM_OBJDIR}/cmake/modules")

find_package(LLVM REQUIRED)

include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})

if(NOT DEFINED CMAKE_C_COMPILER)
	set(CMAKE_C_COMPILER ${LLVM_OBJDIR}/bin/clang)
endif()

if(NOT DEFINED CMAKE_CXX_COMPILER)
	set(CMAKE_CXX_COMPILER ${LLVM_OBJDIR}/bin/clang++)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fno-rtti -std=c++11 -stdlib=libc++")

# these need to be set for set_output_directory() in add_llvm_loadable_module()
set(LLVM_RUNTIME_OUTPUT_INTDIR ${PROJECT_BINARY_DIR}/bin)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${PROJECT_BINARY_DIR}/lib)

set(LIT ${LLVM_OBJDIR}/bin/llvm-lit CACHE FILE "Location of llvm-lit")

include_directories(${LLVM_INCDIR})
include_directories(${LLVM_SRCDIR}/lib)
include_directories(${LLVM_SRCDIR}/runtime/libprofile)
include_directories(${LLVM_OBJDIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/soaap)
link_directories(${LLVM_LIBDIR})

add_subdirectory(soaap)
add_subdirectory(tests)
add_subdirectory(tools)
