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

__soaap_sandbox_persistent("box2")
void domorestuff() {
  int z = sensitive;
  /*
   * CHECK: *** Sandboxed method "domorestuff" read data
   * CHECK:     value belonging to sandboxes: [network]
   * CHECK:     but it executes in sandboxes: [box2]
   */
  printf("secret is: %d\n", z);
  /*
   * CHECK-NOT: *** Sandboxed method "domorestuff" executing in sandboxes: [box2]
   * CHECK-NOT:     may leak private data through the extern function printf
   */
}

__soaap_sandbox_persistent("network")
void dostuff() {
  int y = sensitive;
  printf("secret y is: %d\n", y);
  /*
   * CHECK: *** Sandboxed method "dostuff" executing in sandboxes: [network]
   * CHECK:     may leak private data through the extern function printf
   */
}
