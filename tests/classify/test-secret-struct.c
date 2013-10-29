/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: opt -load libsoaap.so -soaap -o %t.soaap.ll %t.ll | FileCheck %s
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <string.h>

struct cred {
//  char user[10]; 
//  char pass[10] __soaap_classify("secret"); 
  int x __soaap_classify("secret");
};

void doauth(struct cred* c);

int main() {
  struct cred c;
  c.x = 25;
  doauth(&c);
  //strcpy(c.pass, "you");
  return 0;
}

//__soaap_sandbox_persistent
//__soaap_clearance("secret")
void doauth(struct cred* c) {
//  strcpy(c->user, "me");
//  strcpy(c->pass, "orange");
  int y = c->x;
  printf("secret y is: %d\n", y);
}
