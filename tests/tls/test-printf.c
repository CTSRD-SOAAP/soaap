/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: opt -load libsoaap.so -soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include <stdio.h>

int main(int argc, char** argv) {
	printf("%d -> %d\n", 1, 2);
	return 0;
}
