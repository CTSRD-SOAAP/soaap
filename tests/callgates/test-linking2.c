/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"

int b();

__soaap_callgates(sandbox2, b);

int b() {
  return 1;
}
