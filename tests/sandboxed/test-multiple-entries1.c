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

// CHECK: *** Function "f2" has been annotated as only being allowed to execute in the sandboxes: [mybox2] but it executes in the sandboxes: [mybox1] of which [mybox1] are disallowed
// CHECK-NEXT: Possible trace ([mybox1]):
// CHECK-NEXT:      foo ({{.*}}:21)
// CHECK-NEXT:      main ({{.*}}:29)
