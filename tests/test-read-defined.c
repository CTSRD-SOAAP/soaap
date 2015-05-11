/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */

#include "soaap.h"

long read(int fd, void* buf, unsigned long size) {
    return 1;
}

__soaap_sandbox_persistent("sandbox named foo")
void f(int ifd __soaap_fd_permit(read), int ofd) {
  char buf[10];
  // CHECK-NOT: Sandbox "sandbox named foo" performs system call "read" but it is not allowed to
  read(ifd, buf, 1);
  // CHECK-DAG: Sandbox "sandbox named foo" performs system call "read" but is not allowed to
  // CHECK-DAG: Line 22 of file
  read(ofd, buf, 1);
}

int main() {
  // CHECK-DAG: *** Found call to sandbox entrypoint "f" that is not preceded by sandbox creation
  f(3,4);
  return 0;
}
