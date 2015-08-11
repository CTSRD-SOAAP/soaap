/*
 * RUN: clang++ %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"

class A {
  public:
    __soaap_sandbox_persistent("mysandbox")
    void foo() {
      int z __soaap_private("mysandbox");
      z = 3;
      x = z;
    }
    void bar() {
      int y = x;
    }

  private:
    int x;
};

int main(int argc, char** argv) {
  A* a = new A;
  a->foo();
  a->bar();
  return 0;
}
// CHECK: *** Privileged method "_ZN1A3barEv" read data value belonging to sandboxes: [mysandbox]
// CHECK  +++ Line 12 of file {{.*}}

