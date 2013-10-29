/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: opt -load libsoaap.so -soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include <stdio.h>

void a() {
	printf("Hello from a\n");
}

int main(int argc, char** argv) {
	void (*fp)(void);
	fp = &a;
	(*fp)();
	return 0;
}
