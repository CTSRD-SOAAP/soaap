/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <string.h>

void dostuff();
void privfunc(int p1);

__soaap_callgates(box, privfunc);

int main() {
  dostuff();
  return 0;
}

__soaap_sandbox_persistent("box")
void dostuff() {
  int key __soaap_private("box");
  /*
   * CHECK: *** Sandboxed method "dostuff" executing in sandboxes: [box]
   * CHECK:     may leak private data through callgate privfunc
   */
  privfunc(key);
}

void privfunc(int p1) {
  printf("p1: %d\n", p1);
}
