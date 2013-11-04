/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <string.h>

int x;

void dostuff();

int main() {
  dostuff();
  printf("value of secret sandbox-private local variable: %d\n", x);
  return 0;
}

__soaap_sandbox_persistent("network")
void dostuff() {
  int y __soaap_private("network");
  y = 25;
  // CHECK: Sandboxed method "dostuff" executing in sandboxes: [network]
  // CHECK: may leak private data through global variable x
  printf("leaking sandbox-private y to global x\n");
  x = y;
}

