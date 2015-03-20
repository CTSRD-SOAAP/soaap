/* 
 * RUN: clang %cflags -emit-llvm -S %S/Inputs/link.mod.1.c -o %t.1.ll
 * RUN: clang %cflags -emit-llvm -S %S/Inputs/link.mod.2.c -o %t.2.ll
 * RUN: llvm-link -S -module-name=test -o %t.ll %t.1.ll %t.2.ll
 * RUN: FileCheck %s -input-file %t.ll

 * CHECK-DAG: !llvm.module = !{![[MOD:[0-9]+]]}
 * CHECK-DAG: ![[MOD]] = !MDLLVMModule(name: "test", cus: ![[CUS:[0-9]+]])
 * CHECK-DAG: ![[CUS]] = !{![[CU1:[0-9]+]], ![[CU2:[0-9]+]]}
 * CHECK-DAG: ![[CU1]] = !MDCompileUnit
 * CHECK-DAG: ![[CU2]] = !MDCompileUnit
 */
