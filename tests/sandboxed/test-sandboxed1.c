/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap --soaap-output-traces=sandboxed -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"

__soaap_sandboxed("mybox1")
void f1() {
}

__soaap_sandboxed("mybox2")
void f2() {
}

__soaap_sandbox_persistent("mybox1")
void foo() {
  f1();
  f2();
}

__soaap_sandbox_persistent("mybox2")
void bar() {
}

int main(int argc, char** argv) {
  foo();
  return 0;
}

// CHECK: *** Function "f2" has been annotated as only being allowed to execute in the sandboxes: [mybox2] but it executes in the sandboxes: [mybox1] of which [mybox1] are disallowed
// CHECK-NEXT: Possible trace ([mybox1]):
// CHECK-NEXT:      foo ({{.*}}:21)
// CHECK-NEXT:      main ({{.*}}:29)
