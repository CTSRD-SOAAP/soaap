/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.soaap.out
 * RUN: FileCheck %s -input-file %t.soaap.out
 * RUN: clang %t.soaap.ll -o %t
 * RUN: %t | FileCheck %s -check-prefix=RUNTIME
 *
 * CHECK: Running Soaap Pass
 */
#include <stdio.h>

void a() {
	// RUNTIME: Hello from a
	printf("Hello from a\n");
}

int main(int argc, char** argv) {
	void (*fp)(void);
	fp = &a;
	(*fp)();
	return 0;
}
