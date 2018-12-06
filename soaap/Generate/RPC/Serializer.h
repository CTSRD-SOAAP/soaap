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

#ifndef SOAAP_GENERATE_RPC_SERIALIZER_H
#define SOAAP_GENERATE_RPC_SERIALIZER_H

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {
  class Serializer {
    public:
      Serializer(Module& M) : M(M) {};
      void serializeArgument(IRBuilder<>& B, Function* F, unsigned idx, Value* V,
          Value* nvlAddr, InputAnnotation* annotationData,
          std::map<Value*, Value*>& allocatedArgs);
      void serializeGlobal(IRBuilder<>& B, Function* F, unsigned idx, Value* V,
          Value* nvlAddr, std::map<Value*, Value*>& allocatedArgs);

      void deserializeArgument(IRBuilder<>& B, Function* F, unsigned idx,
          Value* nvlAddr, Value* alloc, InputAnnotation* annotationData,
          Value* linkedArg);
      void deserializeGlobal(IRBuilder<>& B, Function* F, unsigned idx,
          Value* nvlAddr, Value* alloc);

      void copyBuffer(IRBuilder<>& B, Function* F, Value* src, Value* dst, Value* len);

      Value* filenoFunc(IRBuilder<>& B, Value* V);
      Value* fdopenFunc(IRBuilder<>& B, Value* V, std::string mode);
      void nvlistAddBinary(IRBuilder<>& B, Value* nvl, std::string name, Value* V,
          Value* L);
      void nvlistAddBool(IRBuilder<>& B, Value* nvl, std::string name, Value* V);
      void nvlistAddDescriptor(IRBuilder<>& B, Value* nvl, std::string name, Value* V);
      void nvlistAddNull(IRBuilder<>& B, Value* nvl, std::string name);
      void nvlistAddNumber(IRBuilder<>& B, Value* nvl, std::string name, Value* V);
      void nvlistAddNumberArray(IRBuilder<>& B, Value* nvl, std::string name, Value* V,
          Value* L);
      void nvlistAddNvlist(IRBuilder<>& B, Value* nvl, std::string name, Value* V);
      void nvlistAddString(IRBuilder<>& B, Value* nvl, std::string name, Value* V);
      Value* nvlistCreate(IRBuilder<>& B);
      Value* nvlistExistsNull(IRBuilder<>& B, Value* nvl, std::string name);
      Value* nvlistGetBinary(IRBuilder<>& B, Value* nvl, std::string name,
          Value* lenPtr);
      Value* nvlistGetDescriptor(IRBuilder<>& B, Value* nvl, std::string name);
      Value* nvlistGetNumber(IRBuilder<>& B, Value* nvl, std::string name);
      Value* nvlistGetNumberArray(IRBuilder<>& B, Value* nvl, std::string name,
          Value* lenPtr);
      Value* nvlistGetNvlist(IRBuilder<>& B, Value* nvl, std::string name);
      Value* nvlistGetString(IRBuilder<>& B, Value* nvl, std::string name);

    private:
      Module& M;
      Function* getFunction(std::string name);
      void serialize(IRBuilder<>& B, Function* F, std::string name, Value* V, Value* nvl,
          InputAnnotation* annotationData,
          std::map<Value*, Value*>& allocatedArgs);
      void deserialize(IRBuilder<>& B, Function* F, std::string name, Value* nvl,
          Value* alloc, InputAnnotation* annotationData, Value* linkedArg);

  };
}

#endif  // SOAAP_GENERATE_RPC_SERIALIZER_H
