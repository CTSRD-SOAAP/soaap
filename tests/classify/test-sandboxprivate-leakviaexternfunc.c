/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
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

__soaap_sandbox_persistent("box")
void dostuff() {
  char* password __soaap_private("box");
  password = "mypass";
  printf("leaking sandbox-private password via printf\n");

  // CHECK: "dostuff" executing in sandboxes: [box]
  // CHECK: may leak private data through the extern function printf
  printf("password is: %s\n", password);
}

