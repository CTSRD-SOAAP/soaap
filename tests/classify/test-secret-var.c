/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: opt -load libsoaap.so -soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <string.h>

int sensitive __soaap_classify("secret");

void dostuff();

int main() {
  sensitive = 25;
  dostuff();
  return 0;
}

__soaap_sandbox_persistent
//__soaap_clearance("secret")
void dostuff() {
  int y = sensitive;
  printf("secret y is: %d\n", y);
}
