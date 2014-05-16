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
      if (F->isDeclaration()) continue;
      if (F->hasAddressTaken()) { // limit to only those funcs that could be fp targets
        funcToIdx[F] = nextIdx;
        idxToFunc[nextIdx] = F;
        nextIdx++;
      }
    }
  }
}

void FPTargetsAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
}

// return the union of from and to
bool FPTargetsAnalysis::performMeet(BitVector from, BitVector& to) {
  //static int counter = 0;
  BitVector oldTo = to;
  to |= from;
  //if (counter++ == 1000) {
  //  dbgs() << from.count() << " " << to.count() << "\n";
  //  counter = 0;
  //}
  return to != oldTo;
}

FunctionSet FPTargetsAnalysis::getTargets(Value* FP) {
  BitVector& vector = state[ContextUtils::SINGLE_CONTEXT][FP];
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

void FPTargetsAnalysis::stateChangedForFunctionPointer(CallInst* CI, const Value* FP, BitVector& newState) {
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
    int idx;
    int numSetBits = newState.count();
    for (int i=0; i<numSetBits; i++) {
      idx = (i == 0) ? newState.find_first() : newState.find_next(idx);
      Function* F = idxToFunc[idx];
      if (F->getFunctionType() != FT || F->isDeclaration()) {
        newState.reset(idx);
      }
    }
  }
  else {
    dbgs() << "Unrecognised FP: " << *FP->getType() << "\n";
  }

  FunctionSet newFuncs = convertBitVectorToFunctionSet(newState);
  CallGraphUtils::addCallees(CI, newFuncs);
}
