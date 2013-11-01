/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll
 * RUN: clang %t.soaap.ll -o %t
 * RUN: %t | FileCheck %s
 */
#include <stdio.h>

int main(int argc, char** argv) {
	// CHECK: 1 -> 2
	printf("%d -> %d\n", 1, 2);
	return 0;
}
