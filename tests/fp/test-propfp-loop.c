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
  int i=0;
  int j=ifd;
  char buf[10];
  // CHECK: +++ Line 19 of file {{.*}}
  read(j, buf, 10);
  while (i < 10) {
    ifd = j+1;
    j = ifd;
    // CHECK: Sandbox "foo" performs system call "read" but {{.*}}is not allowed to
    // CHECK: +++ Line 25 of file {{.*}}
    read(j, buf, 10);
    i++;
  }
}

int main(int argc, char** argv) {
  __soaap_create_persistent_sandbox("foo");
  f(1);
}
