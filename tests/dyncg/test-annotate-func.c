/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
int a() __attribute__((annotate("hello"))) {
	return 0;
}

int main(int argc, char** argv) {
	return a();
}

