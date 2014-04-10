/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include <stdio.h>
#include <unistd.h>
#include "soaap.h"

int x __soaap_var_read("sandbox named foo");
void f();

int main() {
  __soaap_create_persistent_sandbox("sandbox named foo");
  // CHECK-DAG: * Write to shared variable "x" ({{.*}}) outside sandbox in method "main" will not be seen by the sandboxes: [sandbox named foo]
  x = 4;
  f();
  return 0;
}

__soaap_sandbox_persistent("sandbox named foo")
void f()  {
  printf("x: %d\n", x);
}
