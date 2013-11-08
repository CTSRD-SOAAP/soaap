/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"

void dostuff();
void privfunc(int p1);

int main() {
  __soaap_create_persistent_sandbox("box");
  dostuff();
  return 0;
}

__soaap_sandbox_persistent("box")
void dostuff() {
  int key __soaap_private("box");
  privfunc(key);
  /*
   * CHECK: *** Sandbox "box" calls privileged function
   * CHECK:     "privfunc" that they are not allowed to.
   */
}

__soaap_privileged
void privfunc(int p1) {
}
