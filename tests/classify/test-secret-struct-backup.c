/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <string.h>

struct cred {
  char user[10];
  char pass[10] __soaap_classify("secret");
};

void doauth(struct cred* c);

int main() {
  __soaap_create_persistent_sandbox("mysandbox");
  struct cred c;
  doauth(&c);
  return 0;
}

__soaap_sandbox_persistent("mysandbox")
//__soaap_clearance("secret")
void doauth(struct cred* c) {
  char* c2 = c->pass;
  /*
   * CHECK: *** Sandboxed method "doauth" read data
   * CHECK:     value of class: [secret] but only has
   * CHECK:     clearances for: []
   */
}
