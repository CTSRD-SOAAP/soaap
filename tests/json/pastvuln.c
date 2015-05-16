/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll --soaap-report-output-formats=json --soaap-report-file-prefix=%t %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.json
 */
#include "soaap.h"

#include <unistd.h>

void foo(int ifd);
void bar();
void baz();

__soaap_callgates(mysandbox, bar);

int global __soaap_var_read("mysandbox");

int main(int argc, char** argv) {
  foo(1); 
  baz(1);
}

__soaap_sandbox_persistent("mysandbox")
void foo(int ifd __soaap_fd_permit(read)) {
  baz(ifd);
}

void baz(int ifd) {
  int x;
  char buffer[10] __soaap_private("mysandbox");
  bar();
  __soaap_vuln_pt("CVE_1970_XXX")
  if (x) {
    printf("time is: %d\n", 123);
  }
  read(ifd, buffer, 10);
}

void bar() {
}

// CHECK:        "soaap": {
// CHECK-NEXT:     "vulnerability_warning": [
// CHECK-NEXT:       {
// CHECK-NEXT:         "function": "baz",
// CHECK-NEXT:         "sandbox": "mysandbox",
// CHECK-NEXT:         "location": {
// CHECK-NEXT:           "file": "[[TESTS:.*]]/json/pastvuln.c",
// CHECK-NEXT:           "line": 28
// CHECK-NEXT:         },
// CHECK-NEXT:         "type": "cve",
// CHECK-NEXT:         "cve": [
// CHECK-NEXT:           {
// CHECK-NEXT:             "id": "CVE_1970_XXX"
// CHECK-NEXT:           }
// CHECK-NEXT:         ],
// CHECK-NEXT:         "restricted_rights": "true",
// CHECK-NEXT:         "rights_leaked": {
// CHECK-NEXT:           "global": [
// CHECK-NEXT:             {
// CHECK-NEXT:               "global_var": "global",
// CHECK-NEXT:               "perm": [
// CHECK-NEXT:                 {
// CHECK-NEXT:                   "type": "read"
// CHECK-NEXT:                 }
// CHECK-NEXT:               ]
// CHECK-NEXT:             }
// CHECK-NEXT:           ],
// CHECK-NEXT:           "cap_right": [
// CHECK-NEXT:             {
// CHECK-NEXT:               "fd": "ifd",
// CHECK-NEXT:               "entry_point": "foo",
// CHECK-NEXT:               "syscall": [
// CHECK-NEXT:                 {
// CHECK-NEXT:                   "name": "read"
// CHECK-NEXT:                 }
// CHECK-NEXT:               ]
// CHECK-NEXT:             }
// CHECK-NEXT:           ],
// CHECK-NEXT:           "callgate": [
// CHECK-NEXT:             {
// CHECK-NEXT:               "name": "bar"
// CHECK-NEXT:             }
// CHECK-NEXT:           ],
// CHECK-NEXT:           "private": [
// CHECK-NEXT:             {
// CHECK-NEXT:               "type": "local_var",
// CHECK-NEXT:               "name": "buffer"
// CHECK-NEXT:             }
// CHECK-NEXT:           ]
// CHECK-NEXT:         },
// CHECK-NEXT:         "trace_ref": "!trace0"
// CHECK-NEXT:       },
// CHECK-NEXT:       {
// CHECK-NEXT:         "function": "baz",
// CHECK-NEXT:         "sandbox": null,
// CHECK-NEXT:         "location": {
// CHECK-NEXT:           "file": "[[TESTS]]/json/pastvuln.c",
// CHECK-NEXT:           "line": 28
// CHECK-NEXT:         },
// CHECK-NEXT:         "restricted_rights": false,
// CHECK-NEXT:         "type": "cve",
// CHECK-NEXT:         "cve": [
// CHECK-NEXT:           {
// CHECK-NEXT:             "id": "CVE_1970_XXX"
// CHECK-NEXT:           }
// CHECK-NEXT:         ],
// CHECK-NEXT:         "trace_ref": "!trace1"
// CHECK-NEXT:       }
// CHECK-NEXT:     ],
// CHECK-NEXT:     "global_access_warning": [
// CHECK-NEXT:     ],
// CHECK-NEXT:     "global_lost_update": [
// CHECK-NEXT:     ],
// CHECK-NEXT:     "syscall_warning": [
// CHECK-NEXT:     ],
// CHECK-NEXT:     "cap_rights_warning": [
// CHECK-NEXT:     ],
// CHECK-NEXT:     "privileged_call": [
// CHECK-NEXT:     ],
// CHECK-NEXT:     "sandboxed_func": [
// CHECK-NEXT:     ],
// CHECK-NEXT:     "access_origin_warning": [
// CHECK-NEXT:     ],
// CHECK-NEXT:     "classified_warning": [
// CHECK-NEXT:     ],
// CHECK-NEXT:     "private_access": [
// CHECK-NEXT:     ],
// CHECK-NEXT:     "private_leak": [
// CHECK-NEXT:       {
// CHECK-NEXT:         "type": "extern",
// CHECK-NEXT:         "function": "baz",
// CHECK-NEXT:         "callee": "read",
// CHECK-NEXT:         "sandbox_access": [
// CHECK-NEXT:           {
// CHECK-NEXT:             "name": "mysandbox"
// CHECK-NEXT:           }
// CHECK-NEXT:         ],
// CHECK-NEXT:         "location": {
// CHECK-NEXT:           "line": 36,
// CHECK-NEXT:           "file": "[[TESTS]]/json/pastvuln.c"
// CHECK-NEXT:         }
// CHECK-NEXT:       }
// CHECK-NEXT:     ],
// CHECK-NEXT:     "!trace0": {
// CHECK-NEXT:       "name": "!trace0",
// CHECK-NEXT:       "trace": [
// CHECK-NEXT:         {
// CHECK-NEXT:           "function": "foo",
// CHECK-NEXT:           "location": {
// CHECK-NEXT:             "file": "pastvuln.c",
// CHECK-NEXT:             "line": 25
// CHECK-NEXT:           }
// CHECK-NEXT:         },
// CHECK-NEXT:         {
// CHECK-NEXT:           "function": "main",
// CHECK-NEXT:           "location": {
// CHECK-NEXT:             "file": "pastvuln.c",
// CHECK-NEXT:             "line": 19
// CHECK-NEXT:           }
// CHECK-NEXT:         }
// CHECK-NEXT:       ]
// CHECK-NEXT:     },
// CHECK-NEXT:     "!trace1": {
// CHECK-NEXT:       "name": "!trace1",
// CHECK-NEXT:       "trace": [
// CHECK-NEXT:         {
// CHECK-NEXT:           "function": "main",
// CHECK-NEXT:           "location": {
// CHECK-NEXT:             "file": "pastvuln.c",
// CHECK-NEXT:             "line": 20
// CHECK-NEXT:           }
// CHECK-NEXT:         }
// CHECK-NEXT:       ]
// CHECK-NEXT:     }
// CHECK-NEXT:   }
