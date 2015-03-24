/* 
 * RUN: clang %cflags -emit-llvm -S %S/Inputs/link.mod.1.c -o %t.1.ll
 * RUN: clang %cflags -emit-llvm -S %S/Inputs/link.mod.2.c -o %t.2.ll
 * RUN: llvm-link -S -o %t.12.ll %t.1.ll %t.2.ll
 * RUN: clang %cflags -emit-llvm -S %S/Inputs/link.mod.3.c -o %t.3.ll
 * RUN: llvm-link -S -o %t.123.ll %t.12.ll %t.3.ll
 * RUN: FileCheck %s -input-file %t.123.ll
 *
 * CHECK: !llvm.libs = !{![[MOD1:[0-9]+]], ![[MOD2:[0-9]+]]}
 * CHECK: ![[CU1:[0-9]+]] = !MDCompileUnit
 * CHECK: ![[CU2:[0-9]+]] = !MDCompileUnit
 * CHECK: ![[CU3:[0-9]+]] = !MDCompileUnit
 * CHECK: ![[MOD1]] = !{!"{{.*}}", ![[CUS1:[0-9]+]]}
 * CHECK: ![[CUS1]] = !{![[CU1]], ![[CU2]]}
 * CHECK: ![[MOD2]] = !{!"{{.*}}", ![[CUS2:[0-9]+]]}
 * CHECK: ![[CUS2]] = !{![[CU3]]}
 */
