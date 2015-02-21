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
  
  // do something work and then request something from the privileged process:
  __soaap_rpc_send("<privileged>", PrivRequest);
  // wait for reply from the privileged process
  __soaap_rpc_recv_sync("<privileged>", PrivRequestReply);
}

void message_loop() {
  // message loop
  for (;;) {
    __soaap_rpc_recv_sync("sandbox", PrivRequest);    
    __soaap_rpc_send("sandbox", PrivRequestReply);
  }
}

int main(int argc, char** argv) {
  __soaap_create_persistent_sandbox("sandbox");
  __soaap_rpc_send("sandbox", MyMessage);
  message_loop();
  return 0;
}

// CHECK: main (<privileged>) -- MyMessage --> sandbox (handled by OnMyMessage)
// CHECK: message_loop (<privileged>) -- PrivRequestReply --> sandbox (handled by foo)
// CHECK: foo (sandbox) -- PrivRequest --> <privileged> (handled by message_loop)

