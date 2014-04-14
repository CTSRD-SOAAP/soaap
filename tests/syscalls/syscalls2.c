/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <fcntl.h>

void foo();

int main(int argc, char** argv) {
  foo();
  return 0;
}

__soaap_sandbox_persistent("sandbox")
void foo() {
  __soaap_limit_syscalls(read, write, sigreturn, exit, open); // sandbox process using seccomp
  // CHECK-NOT: *** Sandbox "sandbox" performs system call "open" but it is not allowed to,
  // CHECK-NOT: *** based on the current sandboxing restrictions.
  // CHECK-NOT: +++ Line 24 of file {{.*}}
  open("somefile", O_CREAT);
}
