/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include <stdio.h>

int main(int argc, char** argv) {
	char array[105];
	char c = array[argc];
	return 0;
}
