#!/bin/sh

: ${BUILD_TYPE:="Release"}

if [ ! -d "${LLVM_PREFIX}" ]; then
	echo "LLVM_PREFIX not specified"
	echo "Try: LLVM_PREFIX=/path/to/llvm/build $0"
	exit 1
fi

clang="${LLVM_PREFIX}/bin/clang"
include="/usr/local/include"
cppinc="${include}/c++/v1"

if [ ! -d "${cppinc}" ]; then
	echo "No libc++ at ${cppinc}"
	exit 1
fi

mkdir -p ${BUILD_TYPE} || exit 1
cd ${BUILD_TYPE} || exit 1

PATH=${PATH}:${LLVM_PREFIX}/bin \
	cmake \
	-G Ninja \
	-D CMAKE_BUILD_TYPE=${BUILD_TYPE} \
	-D BUILD_SHARED_LIBS=ON \
	-D LLVM_DIR="${LLVM_PREFIX}/share/llvm/cmake" \
	-D CMAKE_C_COMPILER="${clang}" \
	-D CMAKE_CXX_COMPILER="${clang}++" \
	..
