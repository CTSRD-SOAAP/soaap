/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <stdio.h>

void f();
void g(int);
int read(int,char*,int);

int main(int argc, char** argv) {
  f();  
  return 0;
}

void f() {
  int i=0;
  while (i<10) {
    g(i);
    i++;
  }
}

__sandbox_persistent
void g(int __fd_read ifd) {
  char buf[10];
  if (ifd == -1) 
    read(ifd, buf, 10);
}
