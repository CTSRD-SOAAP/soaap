/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: opt -load libsoaap.so -soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <string.h>

void dostuff();

int main() {
  dostuff();
  return 0;
}

__soaap_sandbox_persistent_named("box")
void dostuff() {
  char* password __soaap_sandbox_private("box");
  password = "mypass";
  printf("leaking sandbox-private password via printf\n");
  printf("password is: %s\n", password);
}

