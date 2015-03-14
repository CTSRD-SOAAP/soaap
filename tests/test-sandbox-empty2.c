/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"

int x __soaap_private("net");

__soaap_sandbox_persistent("auth")
void foo() {
  // CHECK: *** Sandboxed method "foo" read data value belonging to sandboxes: [net] but it executes in sandboxes: [auth]
  // CHECK: +++ Line 16 of file {{.*}}
  int y = x;
}

int main(int argc, char** argv) {
  foo();
  return 0;
}
