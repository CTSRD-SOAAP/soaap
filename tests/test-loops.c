/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
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
    /*
     * CHECK: *** Found call to sandbox entrypoint "g" that is not preceded by sandbox creation
     * CHECK-NEXT: Possible trace:
     * CHECK-NEXT:   f(test-loops.c:{{[0-9]+}})
     * CHECK-NEXT:   main(test-loops.c:{{[0-9]+}})
     */
    g(i);
    i++;
  }
}

__soaap_sandbox_persistent("a sandbox named 'g'")
void g(int __soaap_fd_read ifd) {
  char buf[10];
  if (ifd == -1) 
    // CHECK-NOT: Insufficient privileges for "read()" in sandboxed method "g"
    read(ifd, buf, 10);
}
