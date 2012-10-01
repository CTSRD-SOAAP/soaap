cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_COMPILER "clang")
set(CMAKE_CXX_FLAGS "-Wno-c++11-extensions")

# incorporate CMake features provided by LLVM:
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_BINARY_DIR}/share/llvm/cmake")
include(LLVMConfig)
include(AddLLVM)

include_directories( ${LLVM_SOURCE_DIR}/include )
include_directories( ${VALGRIND_BINARY_DIR}/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )

add_llvm_loadable_module(soaap Soaap.cpp)
