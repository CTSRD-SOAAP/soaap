/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include <stdio.h>
#include <unistd.h>
#include "soaap.h"

int x __soaap_var_read("sandbox named foo");
void f();

int main() {
   // CHECK-DAG: *** Found call to sandbox entrypoint "f" that is not preceded by sandbox creation
  f(3,4);
  return 0;
}

__soaap_sandbox_persistent("sandbox named foo")
void f(int __soaap_fd_read ifd, int ofd) {
  printf("hello from sandbox\n");
  char buf[10];
  // CHECK-DAG: Sandboxed method "f" [sandbox named foo] wrote to global variable "x" ({{.*}}) but is not allowed to
  x = 3;
  // CHECK-NOT: Insufficient privileges for "read()" in sandboxed method "f"
  read(ifd, buf, 1);
  // CHECK-DAG: Insufficient privileges for "write()" in sandboxed method "f"
  write(ofd, buf, 1);
}
