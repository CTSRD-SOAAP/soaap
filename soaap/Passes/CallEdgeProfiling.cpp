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

#define DEBUG_TYPE "insert-call-edge-profiling"

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"
#include "llvm/ADT/ilist.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/DebugInfo.h"
#include "llvm/Analysis/ProfileInfoLoader.h"
#include "Transforms/Instrumentation/ProfilingUtils.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/TypeBuilder.h"

#include <iostream>
#include <vector>

using namespace llvm;
using namespace std;

namespace soaap {

  struct CallEdgeProfiling : public ModulePass {

    static char ID;

    CallEdgeProfiling() : ModulePass(ID) {
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      //AU.addRequired<CallGraph>();
      AU.setPreservesAll();
    }

    virtual bool runOnModule(Module& M) {
      outs() << "Inserting call edge profiling instrumentation\n";

      Function *Main = M.getFunction("main");
      if (Main == 0) {
        errs() << "WARNING: cannot insert edge profiling into a module"
               << " with no main function!\n";
        return false;  // No main, no instrumentation!
      }

      outs() << "Adding counters array\n";

      unsigned numFuncs = 0;
      for (Function& F : M.getFunctionList()) {
        if (F.isDeclaration()) continue; // skip functions that are ony declared
        numFuncs++;
      }

      outs() << "Number of functions: " << numFuncs << "\n";

      unsigned numCalls = 0;
      for (Function& F : M.getFunctionList()) {
        for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
          if (isa<CallInst>(*I))
            numCalls++;
        }
      }

      outs() << "Number of calls: " << numCalls << "\n";

      // create 2D array of dimension [numFuncs x numFuncs], to store
      // call edge counts. We don't know which edges will be traversed
      // and so therefore need to create the full NxN array

      Type *ATy = ArrayType::get(Type::getInt32Ty(M.getContext()), (numFuncs+1)*(numCalls+1));

      GlobalVariable *Counters =
          new GlobalVariable(M, ATy, false, GlobalValue::InternalLinkage,
              Constant::getNullValue(ATy), "CallEdgeCounters");

      outs() << "Adding thread-local caller field\n";

      Type* callerVarType = Type::getInt32Ty(M.getContext());
          GlobalVariable* callerVar =
          new GlobalVariable(M, callerVarType, false, GlobalValue::InternalLinkage,
                             Constant::getNullValue(callerVarType), "CallerFunc", NULL, GlobalVariable::GeneralDynamicTLSModel);

      outs() << "Instrumenting all function entries\n";

      unsigned callId = 1;
      unsigned funcId = 1;
      for (Function& F : M.getFunctionList()) {
        if (F.isDeclaration()) continue; // skip functions that are ony declared

        outs() << "Giving function " << F.getName() << " id " << funcId << "\n";

        Constant* FuncIdVal = ConstantInt::get(callerVarType, funcId);

        // set CallerFunc to id, before each call in F
        // and to 0 after each call
        for (BasicBlock& BB : F.getBasicBlockList()) {
          for (Instruction& I : BB.getInstList()) {
            if (isa<CallInst>(&I)) {
              Constant* CallIdVal = ConstantInt::get(callerVarType, callId++);
              new StoreInst(CallIdVal, callerVar, &I);
              Instruction* resetCallerFunc = new StoreInst(ConstantInt::get(Type::getInt32Ty(M.getContext()), 0), callerVar);
              resetCallerFunc->insertAfter(&I);
            }
          }
        }

        // At the function's entry, update caller->callee edge counter
        BasicBlock& BB = F.getEntryBlock();
        BasicBlock::iterator InsertPos = BB.getFirstInsertionPt();
        while (isa<AllocaInst>(InsertPos)) {
          ++InsertPos;
        }


        Value *CallerVal = new LoadInst(callerVar, "CallerVal", InsertPos);

        // Update counter for [CallerVal*(numFuncs+1)+IdVal]
        // Compute array index:
        /*
        Value* CallerValArrayIdx = BinaryOperator::Create(Instruction::Mul, CallerVal,
                        ConstantInt::get(Type::getInt32Ty(M.getContext()), (numFuncs+1)),
                            "CallerValArrayIdx", InsertPos);
        Value* CallerCalleeElemIdx = BinaryOperator::Create(Instruction::Add, CallerValArrayIdx,
                            FuncIdVal,
                            "CallerCalleeElemIdx", InsertPos);
        */

        InsertIncrementCounter(CallerVal, FuncIdVal, numFuncs, InsertPos, M);

        /*
        // Create the GEP instruction
        std::vector<Value*> Indices(2);
        Indices[0] = Constant::getNullValue(Type::getInt32Ty(M.getContext()));
        Indices[1] = CallerCalleeElemIdx;
        GetElementPtrInst* GEP = GetElementPtrInst::Create(Counters, Indices, "", InsertPos);

        // Load and increment valu
        Value *OldVal = new LoadInst(GEP, "OldCallEdgeCounter", InsertPos);
        Value *NewVal = BinaryOperator::Create(Instruction::Add, OldVal,
                                       ConstantInt::get(Type::getInt32Ty(M.getContext()), 1),
                                                 "NewCallEdgeCounter", InsertPos);
        new StoreInst(NewVal, GEP, InsertPos);
        */

        // DEBUG: output caller -> callee (using printf)

        // create prototype for printf
//        Type *SBP = PointerType::get(IntegerType::get(M.getContext(), 8), 0);
//        FunctionType *MTy = FunctionType::get(IntegerType::getInt32Ty(M.getContext()), vector<Type*>(1, SBP), true);
//        Function* printfFn = dyn_cast<Function>(M.getOrInsertFunction("printf", MTy));

        // insert printf call
//        Value* printfFormatString = builder.CreateGlobalStringPtr("%d -> %d\n");
//        vector<Value*> args;
//        args.push_back(printfFormatString);
//        args.push_back(CallerVal);
//        args.push_back(IdVal);
//        CallInst* PrintCall = CallInst::Create(printfFn, args, "", InsertPos);

        funcId++;
      }

      InsertProfilingInitCall(Main, "soaap_start_call_edge_profiling", Counters);
      return true;
    }

    void InsertIncrementCounter(Value* CallerId, Value* FuncId, int numFuncs, BasicBlock::iterator& InsertPos, Module& M) {
      
      FunctionType* IncFuncType = TypeBuilder<void(types::i<32>,types::i<32>,types::i<32>), true>::get(M.getContext());
      Function* IncFunc = cast<Function>(M.getOrInsertFunction("soaap_increment_call_edge_counter", IncFuncType));
      Value* args[] = { CallerId, FuncId, ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), numFuncs) };
      CallInst::Create(IncFunc, args, "", InsertPos);
    }

  };

  char CallEdgeProfiling::ID = 0;
  static RegisterPass<CallEdgeProfiling> X("insert-call-edge-profiling", "Call Edge Profiling Instrumentation Pass", false, false);

  void addPasses(const PassManagerBuilder &Builder, PassManagerBase &PM) {
    PM.add(new CallEdgeProfiling);
  }

  RegisterStandardPasses S(PassManagerBuilder::EP_OptimizerLast, addPasses);
}
