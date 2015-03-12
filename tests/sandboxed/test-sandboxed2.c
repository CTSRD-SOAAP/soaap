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

__soaap_sandbox_persistent("mybox1")
void foo() {
  f1();
}

int main(int argc, char** argv) {
  foo();
  f1();
  return 0;
}
// CHECK: *** Function "f1" has been annotated as only being allowed to execute in the sandboxes: [mybox1] but it may execute in a privileged context
// CHECK-NEXT:  Possible trace ([<privileged>]):
// CHECK-NEXT:        main ({{.*}}:21)
