/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
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
  f(ifd);
}
