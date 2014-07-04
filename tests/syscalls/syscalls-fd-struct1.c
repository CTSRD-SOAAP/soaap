/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"


#include <stdio.h>
#include <unistd.h>

struct foo {
  int x __soaap_fd_permit("parser", read);
};

__soaap_sandbox_persistent("parser")
void foo(struct foo* f) {
  // CHECK-NOT: Sandbox "parser" performs system call "read" but is not allowed to for the given fd arg.
  read(f->x, NULL, 0);
  // CHECK-DAG: Sandbox "parser" performs system call "write" but is not allowed to for the given fd arg.
  // CHECK-DAG: +++ Line 24 of file {{.*}}
  write(f->x, NULL, 0);
}

int main(int arg, char** argv) {
  foo(NULL);
  return 0;
}
