cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_C_COMPILER)
	set(CMAKE_C_COMPILER ${LLVM_BINARY_DIR}/bin/clang)
endif()

if(NOT DEFINED CMAKE_CXX_COMPILER)
	set(CMAKE_CXX_COMPILER ${LLVM_BINARY_DIR}/bin/clang++)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fno-rtti -Wno-c++11-extensions")
set(LIT ${LLVM_BINARY_DIR}/bin/llvm-lit CACHE FILE "Location of llvm-lit")

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})

# incorporate CMake features provided by LLVM:
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_BINARY_DIR}/share/llvm/cmake")
include(LLVMConfig)
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})

include_directories(${LLVM_SOURCE_DIR}/include)
include_directories(${LLVM_SOURCE_DIR}/lib)
include_directories(${LLVM_SOURCE_DIR}/runtime/libprofile)
include_directories(${LLVM_BINARY_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/soaap)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ClassDebugInfoPass)

add_subdirectory(soaap)
add_subdirectory(tests)
