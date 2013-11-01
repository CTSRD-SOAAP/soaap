/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: soaap -soaap-vulnerable-vendors=org.apache.http -o /dev/null %t.ll > %t.vuln.out
 *
 * RUN: FileCheck %s -input-file %t.out -check-prefix=SAFE
 * RUN: FileCheck %s -input-file %t.vuln.out -check-prefix=VULN
 *
 * SAFE: Running Soaap Pass
 * VULN: Running Soaap Pass
 */
#include "soaap.h"

/*
 * SAFE-NOT: org.apache.http is a vulnerable vendor
 * VULN: org.apache.http is a vulnerable vendor
 */
__soaap_provenance("org.apache.http")

int b() {
  return 4;
}

