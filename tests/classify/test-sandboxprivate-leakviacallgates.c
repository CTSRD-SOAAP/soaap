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

__soaap_callgates(privfunc);

int main() {
  dostuff();
  return 0;
}

__soaap_sandbox_persistent_named("box")
void dostuff() {
  int key __soaap_sandbox_private("box");
  privfunc(key);
}

void privfunc(int p1) {
  printf("p1: %d\n", p1);
}
