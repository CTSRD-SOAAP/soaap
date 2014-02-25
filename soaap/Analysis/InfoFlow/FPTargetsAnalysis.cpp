#include "Analysis/InfoFlow/FPTargetsAnalysis.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/DebugInfo.h"
#include "soaap.h"

#include <sstream>

using namespace soaap;

void FPTargetsAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
}

void FPTargetsAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
}

// return the union of from and to
bool FPTargetsAnalysis::performMeet(FunctionSet from, FunctionSet& to) {
  bool change = false;
  for (Function* F : from) {
    if (to.insert(F)) {
      change = true;
    }
  }
  return change;
}

FunctionSet FPTargetsAnalysis::getTargets(Value* FP) {
  return state[ContextUtils::SINGLE_CONTEXT][FP];
}

string FPTargetsAnalysis::stringifyFact(FunctionSet funcs) {
  return CallGraphUtils::stringifyFunctionSet(funcs);
}

void FPTargetsAnalysis::stateChangedForFunctionPointer(CallInst* CI, const Value* FP, FunctionSet& newState) {
  // Filter out those callees that aren't compatible with FP's function type
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
    FunctionSet kill;
    for (Function* F : newState) {
      if (F->getFunctionType() != FT) {
        kill.insert(F);
      }
    }
    for (Function* F : kill) {
      newState.erase(F);
    }
  }
  else {
    dbgs() << "Unrecognised FP: " << *FP->getType() << "\n";
  }

  CallGraphUtils::addCallees(CI, newState);
}
