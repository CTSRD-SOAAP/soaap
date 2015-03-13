/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap --soaap-list-priv-funcs -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"

void bar() { }

void baz() { }

void m() { }

void foo() {
  __soaap_sandboxed_region_start("auth");
  bar();
  __soaap_sandboxed_region_end("auth");
  m();
}

int main(int argc, char** argv) {
  foo();
  baz();
  return 0;
}
// CHECK-DAG:  Privileged methods:
// CHECK-DAG:   main
// CHECK-DAG:   foo
// CHECK-DAG:   m
// CHECK-DAG:   baz
// CHECK-NOT:   bar
