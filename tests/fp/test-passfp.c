/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include <stdio.h>

void f(int ifd) {
  printf("ifd: %d\n", ifd);
}

int main(int argc, char** argv) {
  FILE* input = fopen(argv[1], "r");
  int ifd = fileno(input);
  // CHECK-NOT: ***
  f(ifd);
}
