/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll --soaap-report-output-formats=json --soaap-report-file-prefix=%t %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.json
 * XFAIL: *
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

//CHECK:    "vulnerabilities": [
//CHECK-NEXT:        {
//CHECK-NEXT:            "location": {
//CHECK-NEXT:                "function": "baz",
//CHECK-NEXT:                "filename": "{{.*}}",
//CHECK-NEXT:                "line_number": 28
//CHECK-NEXT:            },
//CHECK-NEXT:            "vuln_vendor": false,
//CHECK-NEXT:            "cves": [
//CHECK-NEXT:                "CVE_1970_XXX"
//CHECK-NEXT:            ],
//CHECK-NEXT:            "sandboxed": true,
//CHECK-NEXT:            "sandbox": "mysandbox",
//CHECK-NEXT:            "call_stack": [
//CHECK-NEXT:                {
//CHECK-NEXT:                    "function": "foo",
//CHECK-NEXT:                    "filename": "{{.*}}",
//CHECK-NEXT:                    "line_number": 25
//CHECK-NEXT:                },
//CHECK-NEXT:                {
//CHECK-NEXT:                    "function": "main",
//CHECK-NEXT:                    "filename": "{{.*}}",
//CHECK-NEXT:                    "line_number": 19
//CHECK-NEXT:                }
//CHECK-NEXT:            ],
//CHECK-NEXT:            "leaks_limited_right": true,
//CHECK-NEXT:            "rights": {
//CHECK-NEXT:                "globals": [
//CHECK-NEXT:                    {
//CHECK-NEXT:                        "var_name": "global",
//CHECK-NEXT:                        "rights": [
//CHECK-NEXT:                            "Read"
//CHECK-NEXT:                        ]
//CHECK-NEXT:                    }
//CHECK-NEXT:                ],
//CHECK-NEXT:                "privates": [
//CHECK-NEXT:                    {
//CHECK-NEXT:                        "var_name": "buffer",
//CHECK-NEXT:                        "var_type": "local"
//CHECK-NEXT:                    }
//CHECK-NEXT:                ],
//CHECK-NEXT:                "callgates": [
//CHECK-NEXT:                    "bar"
//CHECK-NEXT:                ],
//CHECK-NEXT:                "capabilities": [
//CHECK-NEXT:                    {
//CHECK-NEXT:                        "var_name": "ifd",
//CHECK-NEXT:                        "syscalls": [
//CHECK-NEXT:                            "read"
//CHECK-NEXT:                        ]
//CHECK-NEXT:                    }
//CHECK-NEXT:                ]
//CHECK-NEXT:            }
//CHECK-NEXT:        },
//CHECK-NEXT:        {
//CHECK-NEXT:            "location": {
//CHECK-NEXT:                "function": "baz",
//CHECK-NEXT:                "filename": "{{.*}}",
//CHECK-NEXT:                "line_number": 28
//CHECK-NEXT:            },
//CHECK-NEXT:            "vuln_vendor": false,
//CHECK-NEXT:            "cves": [
//CHECK-NEXT:                "CVE_1970_XXX"
//CHECK-NEXT:            ],
//CHECK-NEXT:            "sandboxed": false,
//CHECK-NEXT:            "call_stack": [
//CHECK-NEXT:                {
//CHECK-NEXT:                    "function": "main",
//CHECK-NEXT:                    "filename": "{{.*}}",
//CHECK-NEXT:                    "line_number": 20
//CHECK-NEXT:                }
//CHECK-NEXT:            ],
//CHECK-NEXT:            "leaks_limited_right": false
//CHECK-NEXT:        }
//CHECK-NEXT:    ]
