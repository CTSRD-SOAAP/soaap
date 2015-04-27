/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"

int x = 0;

__soaap_sandbox_persistent("mybox")
void foo() {
  x = 1;
}

__soaap_sandbox_persistent("mybox")
void bar() {
  x = 2;
}

int main(int argc, char** argv) {
  foo();
  bar();
  return 0;
}

// CHECK-DAG: *** Sandboxed method "foo" [mybox] wrote to global variable "x" ({{.*}}) but is not allowed
// CHECK-DAG: *** Sandboxed method "bar" [mybox] wrote to global variable "x" ({{.*}}) but is not allowed
