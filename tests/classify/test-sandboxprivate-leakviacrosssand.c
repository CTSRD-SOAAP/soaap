/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <string.h>

void dostuff1();
void dostuff2(int x);

int main() {
  dostuff1();
  return 0;
}

__soaap_sandbox_persistent_named("get")
void dostuff1() {
  int key = 813;
  printf("leaking sandbox-private password to another sandbox\n");
  dostuff2(key);
}

__soaap_sandbox_persistent_named("auth")
void dostuff2(int x) {
  printf("x: %d\n", x);
}
