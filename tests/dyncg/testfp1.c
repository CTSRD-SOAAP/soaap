/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */

#include <stdio.h>

void execf(void (*func)(void)) {
	func();
}

void a() {
	printf("Hello from a\n");
}

int main(int argc, char** argv) {
	execf(&a);
	return 0;
}
