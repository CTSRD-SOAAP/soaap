/*
 * Copyright (c) 2017 Gabriela Sklencarova
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "Util/SboxGenUtils.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;
using namespace llvm;

Function* SboxGenUtils::insertFunc(std::string name, Module& M, Type* retTy,
    std::vector<Type*> argTypes, bool varArg) {
  FunctionType* funcType = FunctionType::get(retTy, ArrayRef<Type*>(argTypes), varArg);
  Function* func = cast<Function>(M.getOrInsertFunction(name, funcType));
  return func;
}

Function* SboxGenUtils::insertExternFunction(std::string name, Module& M, Type* retTy,
    std::vector<Type*> argTypes) {
  return insertFunc(name, M, retTy, argTypes, false);
}

Function* SboxGenUtils::insertVarArgFunction(std::string name, Module& M, Type* retTy,
    std::vector<Type*> argTypes) {
  return insertFunc(name, M, retTy, argTypes, true);
}

void SboxGenUtils::insertParentFunctions(Module& M) {
  LLVMContext &C = M.getContext();

  Type* i8 = Type::getInt8Ty(C);
  Type* i8ptr = Type::getInt8PtrTy(C);
  Type* i32 = Type::getInt32Ty(C);
  Type* i64 = Type::getInt64Ty(C);
  Type* i64ptr = Type::getInt64PtrTy(C);
  Type* voidTy = Type::getVoidTy(C);

  Type* fileTy = M.getTypeByName(StringRef("struct.__sFILE"));
  if (!fileTy) {
    fileTy = StructType::create(C, StringRef("struct.__sFILE"));
  }

  Type* nvlistTy = M.getTypeByName(StringRef("struct.nvlist"));
  if (!nvlistTy) {
    nvlistTy = StructType::create(C, StringRef("struct.nvlist"));
  }

  Type* nvlistPtrTy = PointerType::get(nvlistTy, 0);
  Type* filePtrTy = PointerType::get(fileTy, 0);

  insertExternFunction("nvlist_add_binary", M, voidTy, {nvlistPtrTy, i8ptr, i8ptr, i64});
  insertExternFunction("nvlist_add_bool", M, voidTy, {nvlistPtrTy, i8ptr, i8});
  insertExternFunction("nvlist_add_descriptor", M, voidTy, {nvlistPtrTy, i8ptr, i32});
  insertExternFunction("nvlist_add_null", M, voidTy, {nvlistPtrTy, i8ptr});
  insertExternFunction("nvlist_add_number", M, voidTy, {nvlistPtrTy, i8ptr, i64});
  insertExternFunction("nvlist_add_number_array", M, voidTy, {nvlistPtrTy, i8ptr, i64ptr,
      i64});
  insertExternFunction("nvlist_add_nvlist", M, voidTy, {nvlistPtrTy, i8ptr, nvlistPtrTy});
  insertExternFunction("nvlist_add_string", M, voidTy, {nvlistPtrTy, i8ptr, i8ptr});

  insertExternFunction("nvlist_create", M, nvlistPtrTy, {i32});
  insertExternFunction("nvlist_destroy", M, voidTy, {nvlistPtrTy});
  insertExternFunction("nvlist_exists_null", M, i8, {nvlistPtrTy, i8ptr});

  insertExternFunction("nvlist_get_binary", M, i8ptr, {nvlistPtrTy, i8ptr, i64ptr});
  insertExternFunction("nvlist_get_descriptor", M, i32, {nvlistPtrTy, i8ptr});
  insertExternFunction("nvlist_get_number", M, i64, {nvlistPtrTy, i8ptr});
  insertExternFunction("nvlist_get_number_array", M, i64ptr, {nvlistPtrTy, i8ptr, 
      i64ptr});
  insertExternFunction("nvlist_get_nvlist", M, nvlistPtrTy, {nvlistPtrTy, i8ptr});
  insertExternFunction("nvlist_get_string", M, i8ptr, {nvlistPtrTy, i8ptr});

  insertExternFunction("fdopen", M, filePtrTy, {i32, i8ptr});
  insertExternFunction("fileno", M, i32, {filePtrTy});

  insertVarArgFunction("printf", M, i32, {i8ptr});
}

void SboxGenUtils::insertSandboxFunctions(Module& M) {
  LLVMContext &C = M.getContext();

  Type* voidTy = Type::getVoidTy(C);
  Type* i8 = Type::getInt8Ty(C);
  Type* i8ptr = Type::getInt8PtrTy(C);
  Type* i32 = Type::getInt32Ty(C);
  Type* i32ptr = Type::getInt32PtrTy(C);
  Type* i64 = Type::getInt64Ty(C);
  Type* i64ptr = PointerType::get(i64, 0);
  Type* i64dptr = PointerType::get(i64ptr, 0);

  Type* fileTy = M.getTypeByName(StringRef("struct.__sFILE"));
  if (!fileTy) {
    fileTy = StructType::create(C, StringRef("struct.__sFILE"));
  }

  Type* nvlistTy = M.getTypeByName(StringRef("struct.nvlist"));
  if (!nvlistTy) {
    nvlistTy = StructType::create(C, StringRef("struct.nvlist"));
  }

  Type* rightsTy = M.getTypeByName(StringRef("struct.cap_rights"));
  if (!rightsTy) {
    rightsTy = StructType::create(C, ArrayRef<Type*>({ArrayType::get(i64, 2)}),
        StringRef("struct.cap_rights_t"));
  }

  Type* filePtrTy = PointerType::get(fileTy, 0);
  Type* nvlistPtrTy = PointerType::get(nvlistTy, 0);
  Type* rightsPtrTy = PointerType::get(rightsTy, 0);

  /* libnv */
  insertExternFunction("nvlist_add_binary", M, voidTy, {nvlistPtrTy, i8ptr, i8ptr, i64});
  insertExternFunction("nvlist_add_bool", M, voidTy, {nvlistPtrTy, i8ptr, i8});
  insertExternFunction("nvlist_add_descriptor", M, voidTy, {nvlistPtrTy, i8ptr, i32});
  insertExternFunction("nvlist_add_null", M, voidTy, {nvlistPtrTy, i8ptr});
  insertExternFunction("nvlist_add_number", M, voidTy, {nvlistPtrTy, i8ptr, i64});
  insertExternFunction("nvlist_add_number_array", M, voidTy, {nvlistPtrTy, i8ptr, i64ptr,
      i64});
  insertExternFunction("nvlist_add_nvlist", M, voidTy, {nvlistPtrTy, i8ptr, nvlistPtrTy});
  insertExternFunction("nvlist_add_string", M, voidTy, {nvlistPtrTy, i8ptr, i8ptr});
  insertExternFunction("nvlist_create", M, nvlistPtrTy, {i32});
  insertExternFunction("nvlist_destroy", M, voidTy, {nvlistPtrTy});
  insertExternFunction("nvlist_exists_null", M, i8, {nvlistPtrTy, i8ptr});
  insertExternFunction("nvlist_get_binary", M, i8ptr, {nvlistPtrTy, i8ptr, i64ptr});
  insertExternFunction("nvlist_get_descriptor", M, i32, {nvlistPtrTy, i8ptr});
  insertExternFunction("nvlist_get_number", M, i64, {nvlistPtrTy, i8ptr});
  insertExternFunction("nvlist_get_number_array", M, i64ptr, {nvlistPtrTy, i8ptr, 
      i64ptr});
  insertExternFunction("nvlist_get_nvlist", M, nvlistPtrTy, {nvlistPtrTy, i8ptr});
  insertExternFunction("nvlist_get_string", M, i8ptr, {nvlistPtrTy, i8ptr});
  insertExternFunction("nvlist_recv", M, nvlistPtrTy, {i32, i32});
  insertExternFunction("nvlist_send", M, i32, {i32, nvlistPtrTy});
  insertExternFunction("nvlist_take_number_array", M, i64dptr,
      {nvlistPtrTy, i8ptr, i64ptr});

  /* Capsicum */
  insertExternFunction("cap_enter", M, i32, {});
  if (!M.getFunction("__cap_rights_init")) {
    insertVarArgFunction("__cap_rights_init", M, rightsPtrTy, {i32, rightsPtrTy});
  }
  if (!M.getFunction("cap_rights_limit")) {
    insertExternFunction("cap_rights_limit", M, i32, {i32, rightsPtrTy});
  }

  /* Other */
  insertExternFunction("__error", M, i32ptr, {});
  insertVarArgFunction("err", M, voidTy, {i32, i8ptr});
  insertExternFunction("fdopen", M, filePtrTy, {i32, i8ptr});
  insertExternFunction("fileno", M, i32, {filePtrTy});
  Function* exitFunc = insertExternFunction("exit", M, voidTy, {i32});

  /* Add the noreturn attribute to exit. */
  exitFunc->addFnAttr(Attribute::NoReturn);
};

std::map<Value*, Value*> SboxGenUtils::allocateArguments(IRBuilder<>& B, Function* F) {
  std::map<Value*, Value*> args;
  for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
    Value* V = dyn_cast<Value>(A);
    args[V] = B.CreateAlloca(A->getType());
  }
  return args;
}

void SboxGenUtils::storeArguments(IRBuilder<>& B, Function* F,
    std::map<Value*, Value*>& ptrs) {
  for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
    Value* V = dyn_cast<Value>(A);
    B.CreateStore(V, ptrs[V]);
  }
}

Type* SboxGenUtils::stripPointerType(Type* T) {
  while (T->isPointerTy()) {
    T = T->getPointerElementType();
  }
  return T;
}
