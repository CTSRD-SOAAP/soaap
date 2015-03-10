/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap --soaap-infer-fp-targets --soaap-list-fp-targets --soaap-context-insens -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"

void (*myfp)();

void store(void (*f)()) {
  myfp = f;
}

void call() {
// CHECK: Function "call"
// CHECK-NEXT:   Call at {{.*}}:23
// CHECK-NEXT:     Targets:
// CHECK-NEXT:       [<single>]:
// CHECK-NEXT:          f1 (inferred)
// CHECK-NEXT:          f2 (inferred)
  myfp();
}

void f1() {
}

void f2() {
}

__soaap_sandbox_persistent("box1")
void sandbox1() {
  store(f1);
  call();
}

__soaap_sandbox_persistent("box2")
void sandbox2() {
  store(f2);
  call();
}

int main(int argc, char** argv) {
  sandbox1();
  sandbox2();
}
