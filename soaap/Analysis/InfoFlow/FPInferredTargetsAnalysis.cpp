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

#include "Analysis/InfoFlow/FPInferredTargetsAnalysis.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/DebugInfo.h"
#include "soaap.h"

#include <sstream>

using namespace soaap;

FunctionSet fpTargetsUniv; // all possible fp targets in the program

void FPInferredTargetsAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
  FPTargetsAnalysis::initialise(worklist, M, sandboxes);

  SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << "Running FP inferred targets analysis\n");

  bool debug = false;
  SDEBUG("soaap.analysis.infoflow.fp.infer", 3, debug = true);
  if (debug) {
    dbgs() << "Program statistics:\n";

    // find all assignments of functions and propagate them!
    long numFuncs = 0;
    long numFPFuncs = 0;
    long numFPcalls = 0;
    long numInsts = 0;
    long numAddFuncs = 0;
    long loadInsts = 0;
    long storeInsts = 0;
    long intrinsInsts = 0;
    for (Function& F : M.functions()) {
      if (F.hasAddressTaken()) numAddFuncs++;
      bool hasFPcall = false;
      for (Instruction& I : instructions(&F)) {
        numInsts++;
        if (IntrinsicInst* II = dyn_cast<IntrinsicInst>(&I)) {
          intrinsInsts++;
        }
        else if (CallInst* C = dyn_cast<CallInst>(&I)) {
          if (CallGraphUtils::isIndirectCall(C)) {
            hasFPcall = true;
            numFPcalls++;
          }
        }
        else if (LoadInst* L = dyn_cast<LoadInst>(&I)) {
          loadInsts++;
        }
        else if (StoreInst* S = dyn_cast<StoreInst>(&I)) {
          storeInsts++;
        }
      }
      if (hasFPcall) {
        numFPFuncs++;
      }
      numFuncs++;
    }
    dbgs() << "Num of funcs (total): " << numFuncs << "\n";
    dbgs() << "Num of funcs (w/ fp calls): " << numFPFuncs << "\n";
    dbgs() << "Num of funcs (addr. taken): " << numAddFuncs << "\n";
    dbgs() << "Num of fp calls: " << numFPcalls << "\n";
    dbgs() << "Num of instructions: " << numInsts << "\n";
    dbgs() << INDENT_1 << "loads: " << loadInsts << "\n";
    dbgs() << INDENT_1 << "stores: " << storeInsts << "\n";
    dbgs() << INDENT_1 << "intrinsics: " << intrinsInsts << "\n";
  }
  
  for (Function& F : M.functions()) {
    if (F.isDeclaration()) continue;
    SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << F.getName() << "\n");
    for (Instruction& I : instructions(&F)) {
      ContextVector contexts = ContextUtils::getContextsForInstruction(&I, contextInsensitive, sandboxes, M);
      if (StoreInst* S = dyn_cast<StoreInst>(&I)) { // assignments
        //SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << F->getName() << ": " << *S);
        Value* Rval = S->getValueOperand()->stripInBoundsConstantOffsets();
        if (Function* T = dyn_cast<Function>(Rval)) {
          fpTargetsUniv.insert(T);
          // we are assigning a function
          Value* Lvar = S->getPointerOperand()->stripInBoundsConstantOffsets();
          for (Context* C : contexts) {
            setBitVector(state[C][Lvar], T);
            SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << "Adding " << Lvar->getName() << " to worklist\n");
            addToWorklist(Lvar, C, worklist);

            if (isa<GetElementPtrInst>(Lvar)) {
              SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << "Rewinding back to alloca\n");
              ValueSet visited;
              propagateToAggregate(Lvar, C, Lvar, visited, worklist, sandboxes, M);
            }
          }
        }
      }
      else if (SelectInst* S = dyn_cast<SelectInst>(&I)) {
        if (Function* F = dyn_cast<Function>(S->getTrueValue()->stripPointerCasts())) {
          addInferredFunction(F, contexts, S, worklist);
        }
        if (Function* F = dyn_cast<Function>(S->getFalseValue()->stripPointerCasts())) {
          addInferredFunction(F, contexts, S, worklist);
        }
      }
      else if (CallInst* C = dyn_cast<CallInst>(&I)) { // passing functions as params
        if (Function* callee = CallGraphUtils::getDirectCallee(C)) {
          int argIdx = 0;
          for (Argument& AI : callee->args()) {
            Value* Arg = C->getArgOperand(argIdx)->stripPointerCasts();
            if (Function* T = dyn_cast<Function>(Arg)) {
              addInferredFunction(T, contexts, &AI, worklist);
            }
            argIdx++;
          }
        }
      }
    }
  }

  SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << "Globals:\n");
  // In some applications, functions are stored within globals aggregates like arrays
  // We search for such arrays conflating any structure contained within
  ValueSet visited;
  for (GlobalVariable& G : M.globals()) {
    findAllFunctionPointersInValue(&G, worklist, visited);
  }

  if (debug) {
    dbgs() << "num of fp targets: " << fpTargetsUniv.size() << "\n";
  }

}

void FPInferredTargetsAnalysis::findAllFunctionPointersInValue(Value* V, ValueContextPairList& worklist, ValueSet& visited) {
  if (!visited.count(V)) {
    visited.insert(V);
    if (GlobalVariable* G = dyn_cast<GlobalVariable>(V)) {
      SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << INDENT_1 << "Global var: " << G->getName() << "\n");
      // don't look in the llvm.global.annotations array
      if (G->getName() != "llvm.global.annotations" && G->hasInitializer()) {
        findAllFunctionPointersInValue(G->getInitializer(), worklist, visited);
      }
    }
    else if (ConstantArray* CA = dyn_cast<ConstantArray>(V)) {
      SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << INDENT_1 << "Constant array, num of operands: " << CA->getNumOperands() << "\n");
      for (int i=0; i<CA->getNumOperands(); i++) {
        Value* V2 = CA->getOperand(i)->stripInBoundsOffsets();
        findAllFunctionPointersInValue(V2, worklist, visited);
      }
    }
    else if (Function* F = dyn_cast<Function>(V)) {
      fpTargetsUniv.insert(F);
      SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << INDENT_1 << "Func: " << F->getName() << "\n");
      setBitVector(state[ContextUtils::NO_CONTEXT][V], F);
      addToWorklist(V, ContextUtils::NO_CONTEXT, worklist);
    }
    else if (ConstantStruct* S = dyn_cast<ConstantStruct>(V)) {
      SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << INDENT_1 << "Struct, num of fields: " << S->getNumOperands() << "\n");
      for (int j=0; j<S->getNumOperands(); j++) {
        Value* V2 = S->getOperand(j)->stripInBoundsOffsets();
        findAllFunctionPointersInValue(V2, worklist, visited);
      }
    }
  }
}

void FPInferredTargetsAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
}

void FPInferredTargetsAnalysis::addInferredFunction(
    Function *F, ContextVector contexts, Value *V,
    ValueContextPairList& worklist) {

  fpTargetsUniv.insert(F);

  for (Context* Ctx : contexts) {
    setBitVector(state[Ctx][V], F);
    addToWorklist(V, Ctx, worklist);
  }
}
