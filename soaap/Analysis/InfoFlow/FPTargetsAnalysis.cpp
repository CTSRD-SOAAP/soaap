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
  string funcNamesStr = "[";
  int currIdx = 0;
  bool first = true;
  for (Function* F : funcs) {
    if (!first)
      funcNamesStr += ",";
    funcNamesStr += F->getName();
    first = false;
  }
  funcNamesStr += "]";
  return funcNamesStr;
}
