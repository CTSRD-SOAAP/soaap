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

int main(int argc, char** argv) {
	char array[105];
	char c = array[argc];
	return 0;
}
