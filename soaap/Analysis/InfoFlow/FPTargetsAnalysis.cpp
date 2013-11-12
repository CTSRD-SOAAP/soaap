#include "Analysis/InfoFlow/FPTargetsAnalysis.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/DebugInfo.h"
#include "soaap.h"

#include <sstream>

using namespace soaap;

void FPTargetsAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
  prevIsContextInsensitiveAnalysis = ContextUtils::IsContextInsensitiveAnalysis;
  ContextUtils::setIsContextInsensitiveAnalysis(true);
}

void FPTargetsAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  // restore cached value of ContextUtils::IsContextInsensitiveAnalysis
  ContextUtils::setIsContextInsensitiveAnalysis(prevIsContextInsensitiveAnalysis);
}

// return the union of from and to
FunctionVector FPTargetsAnalysis::performMeet(FunctionVector from, FunctionVector to) {
  FunctionVector meet = from;
  for (Function* F : to) {
    if (find(meet.begin(), meet.end(), F) == meet.end()) {
      meet.push_back(F);
    }
  }
  return meet;
}

FunctionVector FPTargetsAnalysis::getTargets(Value* FP) {
  return state[ContextUtils::SINGLE_CONTEXT][FP];
}