/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap --soaap-sandbox-platform=annotated -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"

#include <unistd.h>

void foo();

int main(int argc, char** argv) {
  __soaap_create_persistent_sandbox("foo");
  foo();
}

__soaap_sandbox_persistent("foo")
void foo() {
  __soaap_limit_syscalls(read);
  int x = 3;
  switch (x) {
    case 1: {
      printf("x: %d\n", x);
      break;
    };
    case 2: {
      // CHECK: *** Sandbox "foo" performs system call "write" but it is not allowed to,
      // CHECK-NEXT: *** based on the current sandboxing restrictions.
      // CHECK-NEXT: +++ Line 32 of file {{.*}}
      write(1,NULL,0);
      break;
    };
    case 3: {
      read(1,NULL,0);
      break;
    }
    default: {
    }
  }
}
