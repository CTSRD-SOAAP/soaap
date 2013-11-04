/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <string.h>
#include <stdlib.h>

void dostuff();

int main() {
  dostuff();
  printf("value of secret sandbox-private password: %s\n", getenv("PUBLIC"));
  return 0;
}

__soaap_sandbox_persistent("box")
void dostuff() {
  char* password __soaap_private("box");
  password = "mypass";
  printf("leaking sandbox-private password to environment variable PUBLIC\n");

  /*
   * CHECK: Sandboxed method "dostuff" executing in sandboxes: [box]
   * CHECK: may leak private data through env var "PUBLIC"
   */
  setenv("PUBLIC", password, 1);
}

