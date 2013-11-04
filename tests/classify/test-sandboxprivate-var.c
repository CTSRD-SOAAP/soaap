/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <string.h>

int sensitive __soaap_private("network") = 25;

void dostuff();
void domorestuff();

int main() {
  int a = sensitive;
  dostuff();
  domorestuff();
  return 0;
}

__soaap_sandbox_persistent("network")
void dostuff() {

  // CHECK-NOT: Sandboxed method "dostuff" [network] read global variable "sensitive" ({{.*}}) but is not allowed to
  int y = sensitive;
  printf("secret y is: %d\n", y);
}

__soaap_sandbox_persistent("box2")
void domorestuff() {
  // CHECK: Sandboxed method "domorestuff" [box2] read global variable "sensitive" ({{.*}}) but is not allowed to
  int z = sensitive;
  printf("secret is: %d\n", z);
}
