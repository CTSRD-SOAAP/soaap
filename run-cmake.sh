#!/bin/sh

: ${BUILD_TYPE:="Release"}
: ${BUILD_DIR:="Build/${BUILD_TYPE}"}

if [ ! -d "${LLVM_PREFIX}" ]; then
	echo "LLVM_PREFIX not specified"
	echo "Try: LLVM_PREFIX=/path/to/llvm/build $0"
	exit 1
fi

clang="${LLVM_PREFIX}/bin/clang"
include_dirs="/usr/include /usr/local/include"
libcxx=""

for include in ${include_dirs}
do
	cppinc="${include}/c++/v1"
	if [ -d "${cppinc}" ]; then libcxx=${cppinc}; fi
done

if [ "${libcxx}" = "" ]; then
	echo "No libc++ in ${include_dirs}"
	exit 1
fi

rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR} || exit 1
cd ${BUILD_DIR} || exit 1

PATH=${PATH}:${LLVM_PREFIX}/bin \
	cmake \
	-G Ninja \
	-D CMAKE_BUILD_TYPE=${BUILD_TYPE} \
	-D BUILD_SHARED_LIBS=ON \
	-D LLVM_DIR="${LLVM_PREFIX}/share/llvm/cmake" \
	-D CMAKE_C_COMPILER="${clang}" \
	-D CMAKE_CXX_COMPILER="${clang}++" \
	../..
