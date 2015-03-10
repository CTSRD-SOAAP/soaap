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
    for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
      if (F->isDeclaration()) continue;
      if (F->hasAddressTaken()) numAddFuncs++;
      bool hasFPcall = false;
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; I++) {
        numInsts++;
        if (IntrinsicInst* II = dyn_cast<IntrinsicInst>(&*I)) {
          intrinsInsts++;
        }
        else if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          if (CallGraphUtils::isIndirectCall(C)) {
            hasFPcall = true;
            numFPcalls++;
          }
        }
        else if (LoadInst* L = dyn_cast<LoadInst>(&*I)) {
          loadInsts++;
        }
        else if (StoreInst* S = dyn_cast<StoreInst>(&*I)) {
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
  
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << F->getName() << "\n");
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      ContextVector contexts = ContextUtils::getContextsForInstruction(&*I, contextInsensitive, sandboxes, M);
      if (StoreInst* S = dyn_cast<StoreInst>(&*I)) { // assignments
        //SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << F->getName() << ": " << *S);
        Value* Rval = S->getValueOperand()->stripInBoundsConstantOffsets();
        if (Function* T = dyn_cast<Function>(Rval)) {
          if (!T->isDeclaration()) {
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
      }
      else if (SelectInst* S = dyn_cast<SelectInst>(&*I)) {
        if (Function* F = dyn_cast<Function>(S->getTrueValue()->stripPointerCasts())) {
          if (!F->isDeclaration()) {
            fpTargetsUniv.insert(F);
            for (Context* C : contexts) {
              setBitVector(state[C][S], F);
              addToWorklist(S, C, worklist);
            }
          }
        }
        if (Function* F = dyn_cast<Function>(S->getFalseValue()->stripPointerCasts())) {
          if (!F->isDeclaration()) {
            fpTargetsUniv.insert(F);
            for (Context* C : contexts) {
              setBitVector(state[C][S], F);
              addToWorklist(S, C, worklist);
            }
          }
        }
      }
      else if (CallInst* C = dyn_cast<CallInst>(&*I)) { // passing functions as params
        if (Function* callee = CallGraphUtils::getDirectCallee(C)) {
          int argIdx = 0;
          for (Function::arg_iterator AI=callee->arg_begin(), AE=callee->arg_end(); AI != AE; AI++, argIdx++) {
            Value* Arg = C->getArgOperand(argIdx)->stripPointerCasts();
            Value* Param = &*AI;
            if (Function* T = dyn_cast<Function>(Arg)) {
              if (!T->isDeclaration()) {
                fpTargetsUniv.insert(T);
                SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << "Adding param " << Param->getName() << " to worklist\n");
                for (Context* Ctx : contexts) {
                  setBitVector(state[Ctx][Param], T);
                  addToWorklist(Param, Ctx, worklist);
                }
              }
            }
          }
        }
      }
    }
  }

  SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << "Globals:\n");
  // In some applications, functions are stored within globals aggregates like arrays
  // We search for such arrays conflating any structure contained within
  ValueSet visited;
  for (Module::global_iterator G = M.global_begin(), E = M.global_end(); G != E; ++G) {
    findAllFunctionPointersInValue(G, worklist, visited);
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
      if (!F->isDeclaration()) {
        fpTargetsUniv.insert(F);
        SDEBUG("soaap.analysis.infoflow.fp.infer", 3, dbgs() << INDENT_1 << "Func: " << F->getName() << "\n");
        setBitVector(state[ContextUtils::NO_CONTEXT][V], F);
        addToWorklist(V, ContextUtils::NO_CONTEXT, worklist);
      }
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
