/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll --soaap-report-output-formats=json --soaap-report-file-prefix=%t %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.json
 * XFAIL: *
 */
#include "soaap.h"

#include <unistd.h>

void foo();
void bar();
void baz();

int main(int argc, char** argv) {
  foo();
  bar();
}

void foo() {
  bar();
  bar();
}

void bar() {
  baz();
  baz();
  baz();
}

void baz() {
}

//CHECK: "callgraph": [
//CHECK-NEXT:     {
//CHECK-NEXT:         "caller": "main",
//CHECK-NEXT:         "callees": [
//CHECK-NEXT:             {
//CHECK-NEXT:                 "callee": "foo",
//CHECK-NEXT:                 "call_count": 1
//CHECK-NEXT:             }
//CHECK-NEXT:         ]
//CHECK-NEXT:     },
//CHECK-NEXT:     {
//CHECK-NEXT:         "caller": "foo",
//CHECK-NEXT:         "callees": [
//CHECK-NEXT:             {
//CHECK-NEXT:                 "callee": "bar",
//CHECK-NEXT:                 "call_count": 2
//CHECK-NEXT:             }
//CHECK-NEXT:         ]
//CHECK-NEXT:     },
//CHECK-NEXT:     {
//CHECK-NEXT:         "caller": "bar",
//CHECK-NEXT:         "callees": [
//CHECK-NEXT:             {
//CHECK-NEXT:                 "callee": "baz",
//CHECK-NEXT:                 "call_count": 3
//CHECK-NEXT:             }
//CHECK-NEXT:         ]
//CHECK-NEXT:     }
//CHECK-NEXT: ]
