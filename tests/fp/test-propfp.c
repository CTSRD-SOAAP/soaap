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

void g(int i) {
  //printf("i: %d\n", i);
  char buf[10];
  // CHECK: Insufficient privileges for "read()" in sandboxed method "g"
  read(i,buf,10);
}

__soaap_sandbox_persistent("foo")
void f(int __soaap_fd_read ifd) {
  g(ifd);
}

int main(int argc, char** argv) {
  int i = 1;
  //int j = i;
  f(i);
}
