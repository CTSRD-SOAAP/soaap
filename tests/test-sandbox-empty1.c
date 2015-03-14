/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"

void bar();

__soaap_callgates(box, bar);

int x __soaap_private("net");
int y __soaap_var_read("auth");

void bar() {
}

void foo() {
  x = 1;
  bar();
}

int main(int argc, char** argv) {
  foo();
  return 0;
}

// CHECK: Assigning index 0 to sandbox name "net"
// CHECK: Assigning index 1 to sandbox name "auth"
// CHECK: Assigning index 2 to sandbox name "box"
