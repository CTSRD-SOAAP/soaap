/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include <stdio.h>

int f() __attribute__((constructor));

int f() {
	printf("constructor");
}

int main(int argc, char** argv) {
	printf("main\n");
	return 0;
}
