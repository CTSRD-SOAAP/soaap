#include "soaap.h"

/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
int x = 0; 
int y = 1;

__soaap_sandbox_persistent("mysandbox")
void foo() {
  // CHECK: *** Sandboxed method "foo" [mysandbox] read global variable "x"
  // CHECK-NEXT: +++ Line [[@LINE+1]] of file {{.*}}
  int i = x;
  // CHECK: *** Sandboxed method "foo" [mysandbox] read global variable "y" ({{.*}}:10)
  // CHECK-NEXT: +++ Line [[@LINE+1]] of file {{.*}}
  int j = y;
  i++;
  j++;
  // CHECK: *** Sandboxed method "foo" [mysandbox] wrote to global variable "x" ({{.*}}:9)
  // CHECK: +++ Line [[@LINE+1]] of file {{.*}}
  x = i;
  // CHECK: *** Sandboxed method "foo" [mysandbox] wrote to global variable "y" ({{.*}}:10)
  // CHECK: +++ Line [[@LINE+1]] of file {{.*}}
  y = j;
}

int main(int argc, char** argv) {
  __soaap_create_persistent_sandbox("mysandbox");
  foo();
  return 0;
}
