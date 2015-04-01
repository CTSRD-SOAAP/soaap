/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"

extern int read(int,char*,int);

void f(int x) {
  // This used to crash due to endless recursion, make sure it runs without crashing
  __soaap_sandboxed_region_start("box");
  for (int i = 0; i < x; i++) {
    char buf[10];
    read(0, buf, 10);
  }
  __soaap_sandboxed_region_end("box");
  // CHECK:    Assigning index 0 to sandbox name "box"
}

int main(int argc, char** argv) {
  f(10);
  return 0;
}
