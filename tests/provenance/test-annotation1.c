/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"

__soaap_provenance("org.gnu.libc")

int b();

int a() {
  return 3;
}

int main() {
  a();
  b();
}
