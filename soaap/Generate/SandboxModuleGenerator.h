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

#ifndef SOAAP_GENERATE_SANDBOX_MODULE_GENERATOR_H
#define SOAAP_GENERATE_SANDBOX_MODULE_GENERATOR_H

#include "Common/Sandbox.h"
#include "Generate/RPC/Serializer.h"
#include "Util/SboxGenUtils.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

using namespace llvm;

namespace soaap {
  class SandboxModuleGenerator {
    public:
      SandboxModuleGenerator(Module& O, Sandbox& S);
      Module* get();
    private:
      Module& O;
      Sandbox& S;
      Module* M;
      GlobalVariable* mainFd;
      ValueToValueMapTy VMap;
      map<string, pair<Value*, Value*>> handleTypesMap;
      map<string, Function*> getters;
      map<string, Function*> setters;

      void generate();
      void generateMainFunction();
      void generateGetterFunction();
      void getterSwitchHelper(Module* M, IRBuilder<>& B, Serializer& R,
          Function* F, string fname, StructType* ST, SwitchInst* switchInst,
          Value* handlePtr, Value* res, Value* pathAddr, unsigned depth);
      void setterSwitchHelper(Module* M, IRBuilder<>& B, Serializer& R,
          Function* F, string fname, StructType* ST, SwitchInst* switchInst,
          Value* handlePtr, Value* storeVal, Value* res, Value* pathAddr, unsigned depth);
      void generateSetterFunction();
      void generateDispatchFunction();
      void limitDescriptorRights(IRBuilder<>& B, Value* arg, Value* fd,
          std::string funcName, Function* dispatch);
      Function* generateStructMethod(StructType* ST, unsigned elemIdx, unsigned argIdx);
      void generateErrorMessage(LLVMContext& C, IRBuilder<>& B, std::string message);
  };
}

#endif  // SOAAP_GENERATE_SANDBOX_MODULE_GENERATOR_H
