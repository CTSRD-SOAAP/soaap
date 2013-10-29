/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.main.ll
 * RUN: clang %cflags -emit-llvm -S Inputs/a.c -o %t.a.ll
 * RUN: clang %cflags -emit-llvm -S Inputs/b.c -o %t.b.ll
 * RUN: llvm-link -O4 %t.main.ll %t.a.ll %t.b.ll -o %t.ll
 * RUN: opt -load libsoaap.so -soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include <stdio.h>

extern int x;
extern int y;

extern void f();
extern void g();

int main(int argc, char** argv) {
	f();
	g();
	printf("x: %d, y: %d\n", x, y);
	x = y;
	return 0;
}
