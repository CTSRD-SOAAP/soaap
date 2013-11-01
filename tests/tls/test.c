/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * This test simply checks that SOAAP runs correctly (e.g. doesn't segfault).
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
