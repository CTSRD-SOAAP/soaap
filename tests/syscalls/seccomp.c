/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap --soaap-infer-fp-targets --soaap-sandbox-platform=seccomp -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 * XFAIL: *
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <fcntl.h>
#include <unistd.h>

void foo();
void bar();
void (*fp)(void) __soaap_var_read("sandbox");

int main(int argc, char** argv) {
  fp = bar;
  foo();
  return 0;
}

__soaap_sandbox_persistent("sandbox")
void foo() {
  fp = bar;
  // CHECK: *** Sandbox "sandbox" performs system call "open" but it is not allowed to,
  // CHECK-NEXT: *** based on the current sandboxing restrictions.
  // CHECK-NEXT: +++ Line 32 of file {{.*}}
  fp();
}

void bar() {
  open("somefile", O_CREAT);
  // CHECK: Sandbox "sandbox" performs system call "read" but is not allowed to for the given fd arg
  // CHECK-NEXT: +++ Line 35 of file {{.*}}
  read(2, NULL, 0);
}
