/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <fcntl.h>
#include <unistd.h>

void foo();

int main(int argc, char** argv) {
  foo();
  return 0;
}

__soaap_sandbox_persistent("sandbox")
void foo() {
  __soaap_limit_syscalls(read, write);
  __soaap_limit_fd_syscalls(STDOUT_FILENO, read);
  // CHECK-DAG: *** Sandbox "sandbox" performs system call "write" but is not allowed to for the given fd arg.
  // CHECK-DAG: +++ Line 25 of file {{.*}}
  write(STDOUT_FILENO, NULL, 0);
  // CHECK-NOT: *** Sandbox "sandbox" performs system call "read" but is not allowed to for the given fd arg.
  // CHECK-NOT: +++ Line 28 of file {{.*}}
  read(STDOUT_FILENO, NULL, 0);
}
