/*
 * RUN: clang %cflags -emit-llvm -S %S/Inputs/link.mod.warn.1.c -o %t.1.ll
 * RUN: clang %cflags -emit-llvm -S %S/Inputs/link.mod.warn.2.c -o %t.2.ll
 * RUN: llvm-link -o %t.ll %t.1.ll %t.2.ll
 * RUN: soaap --soaap-nowarn-libs=modinfo-nowarn1.c.tmp.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 * CHECK-NOT: wrote to global variable
 */
