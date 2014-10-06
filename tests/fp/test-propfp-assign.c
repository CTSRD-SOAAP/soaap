/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include <stdio.h>
#include "soaap.h"

int read(int,char*,int);

__soaap_sandbox_persistent("foo")
void f(int __soaap_fd_read ifd) {
  char buf[10];
  int j = ifd;
  ifd = j+1;
  // CHECK: Sandbox "foo" performs system call "read" but is not allowed to
  read(ifd, buf, 10);
}

int main(int argc, char** argv) {
  __soaap_create_persistent_sandbox("foo");
  f(1);
}
