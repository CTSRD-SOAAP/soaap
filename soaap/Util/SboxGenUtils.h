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

#ifndef SOAAP_UTILS_SBOXGENUTILS_H
#define SOAAP_UTILS_SBOXGENUTILS_H

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

#include "Common/Sandbox.h"

using namespace llvm;

namespace soaap {
  class SboxGenUtils {
    public:
      static Function* insertExternFunction(std::string name, Module& M, Type* retTy,
          std::vector<Type*> argTypes);
      static Function* insertVarArgFunction(std::string name, Module& M, Type* retTy,
          std::vector<Type*> argTypes);
      static void insertParentFunctions(Module& M);
      static void insertSandboxFunctions(Module& M);
      static std::map<Value*, Value*> allocateArguments(IRBuilder<>& B, Function* F);
      static void storeArguments(IRBuilder<>& B, Function* F,
          std::map<Value*, Value*>& ptrs);
      static bool isConstructedType(Sandbox& S, Type* T);
      static bool isConstructedBy(Sandbox& S, Type* T, Function* F);
      static bool isHandleType(Sandbox& S, Type* T);
      static bool isHandleReturnedBy(Sandbox& S, Type* T, Function* F);
      static std::string getConstructedTypeName(Sandbox& S, Type* T);
      static std::string getHandleTypeName(Sandbox& S, Type* T);
      static Type* stripPointerType(Type* T);
    private:
      static Function* insertFunc(std::string name, Module& M, Type* retTy,
          std::vector<Type*> argTypes, bool varArg);
  };
}

#endif
