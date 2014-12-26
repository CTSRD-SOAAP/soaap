/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
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

__soaap_sandbox_persistent("get")
void dostuff1() {
  int key = 813;
  printf("leaking sandbox-private password to another sandbox\n");

  // CHECK: "dostuff1" executing in sandboxes: [get]
  // CHECK: may leak private data through a cross-sandbox call into [auth]
  dostuff2(key);
}

__soaap_sandbox_persistent("auth")
void dostuff2(int x) {
  printf("x: %d\n", x);
}
