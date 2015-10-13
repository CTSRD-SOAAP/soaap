/*
 * Copyright (c) 2013-2015 Khilan Gudka
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

#ifndef SOAAP_ANALYSIS_INFOFLOW_FPTARGETSANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_FPTARGETSANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

#include "llvm/ADT/BitVector.h"

using namespace llvm;

namespace soaap {
  class FPTargetsAnalysis: public InfoFlowAnalysis<BitVector> {
    public:
      FPTargetsAnalysis(bool contextInsens) : InfoFlowAnalysis<BitVector>(contextInsens, false) { }
      virtual FunctionSet getTargets(Value* FP, Context* C);
      virtual bool hasTargets() { return !state.empty(); } // TODO: should we be looking inside state?

    protected:
      static map<Function*,int> funcToIdx;
      static map<int,Function*> idxToFunc;
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) = 0;
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual bool performMeet(BitVector from, BitVector& to);
      virtual bool performUnion(BitVector from, BitVector& to);
      virtual BitVector bottomValue() { return BitVector(); }
      virtual bool checkEqual(BitVector f1, BitVector f2) { return f1 == f2; }
      virtual string stringifyFact(BitVector fact);
      virtual void stateChangedForFunctionPointer(CallInst* CI, const Value* FP, Context* C, BitVector& newState);
      virtual FunctionSet convertBitVectorToFunctionSet(BitVector vector);
      virtual BitVector convertFunctionSetToBitVector(FunctionSet funcs);
      virtual void setBitVector(BitVector& vector, Function* F);
      virtual bool areTypeCompatible(FunctionType* FT1, FunctionType* FT2);
  };
}

#endif 
