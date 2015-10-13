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

#include "Analysis/InfoFlow/FPTargetsAnalysis.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "soaap.h"

#include <sstream>

using namespace soaap;

map<Function*,int> FPTargetsAnalysis::funcToIdx;
map<int,Function*> FPTargetsAnalysis::idxToFunc;

void FPTargetsAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
  // iniitalise funcToIdx and idxToFunc maps (once)
  if (funcToIdx.empty()) {
    int nextIdx = 0;
    for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
      if (F->hasAddressTaken()) { // limit to only those funcs that could be fp targets
        SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "Adding " << F->getName() << " as address taken\n");
        funcToIdx[F] = nextIdx;
        idxToFunc[nextIdx] = F;
        nextIdx++;
      }
      else {
        SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "Skipping " << F->getName() << " as address not taken\n");
      }
    }
  }
}

void FPTargetsAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
}

// return the union of from and to
bool FPTargetsAnalysis::performMeet(BitVector from, BitVector& to) {
  return performUnion(from, to);
}

// return the union of from and to
bool FPTargetsAnalysis::performUnion(BitVector from, BitVector& to) {
  //static int counter = 0;
  BitVector oldTo = to;
  to |= from;
  return to != oldTo;
}

FunctionSet FPTargetsAnalysis::getTargets(Value* FP, Context* C) {
  BitVector& vector = state[C][FP];
  return convertBitVectorToFunctionSet(vector);
}

FunctionSet FPTargetsAnalysis::convertBitVectorToFunctionSet(BitVector vector) {
  FunctionSet functions;
  int idx = 0;
  for (int i=0; i<vector.count(); i++) {
    idx = (i == 0) ? vector.find_first() : vector.find_next(idx);
    functions.insert(idxToFunc[idx]);
  }
  return functions;
}

BitVector FPTargetsAnalysis::convertFunctionSetToBitVector(FunctionSet funcs) {
  BitVector vector;
  for (Function* F : funcs) {
    setBitVector(vector, F);
  }
  return vector;
}

void FPTargetsAnalysis::setBitVector(BitVector& vector, Function* F) {
  int idx = funcToIdx[F];
  if (vector.size() <= idx) {
    vector.resize(idx+1);
  }
  vector.set(idx);
}

string FPTargetsAnalysis::stringifyFact(BitVector fact) {
  FunctionSet funcs = convertBitVectorToFunctionSet(fact);
  return CallGraphUtils::stringifyFunctionSet(funcs);
}

void FPTargetsAnalysis::stateChangedForFunctionPointer(CallInst* CI, const Value* FP, Context* C, BitVector& newState) {
  // Filter out those callees that aren't compatible with FP's function type
  SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "bits set (before): " << newState.count() << "\n");
  FunctionType* FT = NULL;
  if (PointerType* PT = dyn_cast<PointerType>(FP->getType())) {
    if (PointerType* PT2 = dyn_cast<PointerType>(PT->getElementType())) {
      FT = dyn_cast<FunctionType>(PT2->getElementType());
    }
    else {
      FT = dyn_cast<FunctionType>(PT->getElementType());
    }
  }
  
  if (FT != NULL) {
    int idx;
    int numSetBits = newState.count();
    for (int i=0; i<numSetBits; i++) {
      idx = (i == 0) ? newState.find_first() : newState.find_next(idx);
      Function* F = idxToFunc[idx];
      SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "F: " << F->getName() << "\n");
      SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "FT: " << *FT << "\n");
      if (!areTypeCompatible(F->getFunctionType(), FT)) {
        newState.reset(idx);
        FunctionType* FT2 = F->getFunctionType();
        SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "Function types don't match: " << *FT2 << "\n");
        SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "FT2.return: " << *(FT2->getReturnType()) << "\n");
        SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "FT2.params: " << FT2->getNumParams() << "\n");
        SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "FT2.varargs: " << FT2->isVarArg() << "\n");
        SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "FT.vararg: " << FT->isVarArg() << "\n");
      }
    }
  }
  else {
    dbgs() << "Unrecognised FP: " << *FP->getType() << "\n";
  }
  SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "bits set (after): " << newState.count() << "\n");
  FunctionSet newFuncs = convertBitVectorToFunctionSet(newState);
  CallGraphUtils::addCallees(CI, C, newFuncs, true);
}

bool FPTargetsAnalysis::areTypeCompatible(FunctionType* FT1, FunctionType* FT2) {
  SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "checking " << *FT1 << " with " << *FT2 << "\n");
  if (FT1 == FT2) {
    SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "same\n");
    return true;
  }
  else if (FT1->isVarArg() || FT2->isVarArg()) {
    // check return and parameter types
    SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "at least one has varargs, checking return type and param types\n");
    if (FT1->getReturnType() == FT2->getReturnType()
           && FT1->getNumParams() == FT2->getNumParams()) {
      int i=0;
      for (Type* T : FT1->params()) {
        if (T != FT1->getParamType(i)) {
          SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "mismatch at index " << i << "\n");
          return false;
        }
        i++;
      }
      return true;
    }
  }
  SDEBUG("soaap.analysis.infoflow.fp", 3, dbgs() << "not type compatible\n");
  return false;
}
