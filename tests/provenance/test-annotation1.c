/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: soaap -soaap-vulnerable-vendors=org.gnu.libc -o /dev/null %t.ll > %t.vuln.out
 *
 * RUN: FileCheck %s -input-file %t.out -check-prefix=SAFE
 * RUN: FileCheck %s -input-file %t.vuln.out -check-prefix=VULN
 *
 * SAFE: Running Soaap Pass
 * VULN: Running Soaap Pass
 */
#include "soaap.h"

/*
 * SAFE-NOT: org.gnu.libc is a vulnerable vendor
 * VULN: org.gnu.libc is a vulnerable vendor
 */
__soaap_provenance("org.gnu.libc")

int b();

int a() {
  return 3;
}

int main() {
  a();
  b();
}
