/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap --soaap-dump-rpc-graph -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */

#include "soaap.h"

void OnMyMessage() {
}

__soaap_sandbox_persistent("sandbox")
void foo() {
  __soaap_rpc_recv("<privileged>", MyMessage, OnMyMessage);
}

int main(int argc, char** argv) {
  __soaap_create_persistent_sandbox("sandbox");
  __soaap_rpc_send("sandbox", MyMessage);
  return 0;
}

// CHECK-DAG: main (<privileged>) ---MyMessage--> sandbox (handled by OnMyMessage)
