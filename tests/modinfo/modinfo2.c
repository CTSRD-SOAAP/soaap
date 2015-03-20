/* 
 * RUN: clang %cflags -emit-llvm -S %S/Inputs/link.mod.1.c -o %t.1.ll
 * RUN: clang %cflags -emit-llvm -S %S/Inputs/link.mod.2.c -o %t.2.ll
 * RUN: llvm-link -S -module-name=test -o %t.12.ll %t.1.ll %t.2.ll
 * RUN: clang %cflags -emit-llvm -S %S/Inputs/link.mod.3.c -o %t.3.ll
 * RUN: llvm-link -S -module-name=test2 -o %t.123.ll %t.12.ll %t.3.ll
 * RUN: FileCheck %s -input-file %t.123.ll
 *
 * CHECK-DAG: !llvm.module = !{![[MOD:[0-9]+]]}
 * CHECK-DAG: ![[MOD]] = !MDLLVMModule(name: "test2", modules: ![[MODS:[0-9]+]], cus: ![[CUS:[0-9]+]])
 * CHECK-DAG: ![[MODS]] = !{![[MOD1:[0-9]+]]}
 * CHECK-DAG: ![[MOD1]] = !MDLLVMModule(name: "test", cus: ![[CUS2:[0-9]+]])
 * CHECK-DAG: ![[CUS2]] = !{![[CU1:[0-9]+]], ![[CU2:[0-9]+]]}
 * CHECK-DAG: ![[CU1]] = !MDCompileUnit
 * CHECK-DAG: ![[CU2]] = !MDCompileUnit
 * CHECK-DAG: ![[CUS]] = !{![[CU3:[0-9]+]]}
 * CHECK-DAG: ![[CU3]] = !MDCompileUnit
 */
