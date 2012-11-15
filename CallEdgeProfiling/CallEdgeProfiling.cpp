#define DEBUG_TYPE "insert-call-edge-profiling"

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"
#include "llvm/ADT/ilist.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Constants.h"
#include "llvm/Type.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/IRBuilder.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instruction.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/DebugInfo.h"
#include "llvm/Analysis/ProfileInfoLoader.h"
#include "llvm/Constants.h"
#include "Transforms/Instrumentation/ProfilingUtils.h"
#include "llvm/Support/InstIterator.h"

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
      AU.addRequired<CallGraph>();
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
        Value* CallerValArrayIdx = BinaryOperator::Create(Instruction::Mul, CallerVal,
                        ConstantInt::get(Type::getInt32Ty(M.getContext()), (numFuncs+1)),
                            "CallerValArrayIdx", InsertPos);
        Value* CallerCalleeElemIdx = BinaryOperator::Create(Instruction::Add, CallerValArrayIdx,
                            FuncIdVal,
                            "CallerCalleeElemIdx", InsertPos);

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

      InsertProfilingInitCall(Main, "llvm_start_call_edge_profiling", Counters);
      return true;
    }

  };

  char CallEdgeProfiling::ID = 0;
  static RegisterPass<CallEdgeProfiling> X("insert-call-edge-profiling", "Call Edge Profiling Instrumentation Pass", false, false);

  void addPasses(const PassManagerBuilder &Builder, PassManagerBase &PM) {
    PM.add(new CallEdgeProfiling);
  }

  RegisterStandardPasses S(PassManagerBuilder::EP_OptimizerLast, addPasses);
}
