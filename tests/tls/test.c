/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include <stdio.h>

__thread int i;
__thread int j = 1;

int main(int argc, char** argv) {
	i = 1;
	printf("i: %d\n", i);
	return 0;
}
