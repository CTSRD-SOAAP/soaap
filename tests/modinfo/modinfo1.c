/* 
 * RUN: clang %cflags -emit-llvm -S %S/Inputs/link.mod.1.c -o %t.1.ll
 * RUN: clang %cflags -emit-llvm -S %S/Inputs/link.mod.2.c -o %t.2.ll
 * RUN: llvm-link -libmd -S -o %t.ll %t.1.ll %t.2.ll
 * RUN: FileCheck %s -input-file %t.ll

 * CHECK: !llvm.libs = !{![[MOD:[0-9]+]]}
 * CHECK: ![[CU1:[0-9]+]] = distinct !DICompileUnit
 * CHECK: ![[CU2:[0-9]+]] = distinct !DICompileUnit
 * CHECK: ![[MOD]] = !{!"{{.*}}", ![[CUS:[0-9]+]]}
 * CHECK: ![[CUS]] = !{![[CU1]], ![[CU2]]}
 */
