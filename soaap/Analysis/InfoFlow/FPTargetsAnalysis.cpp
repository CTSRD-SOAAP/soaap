#include "Analysis/InfoFlow/FPTargetsAnalysis.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/DebugInfo.h"
#include "soaap.h"

#include <sstream>

using namespace soaap;

void FPTargetsAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
  ContextUtils::startContextInsensitiveAnalysis();
}

void FPTargetsAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  ContextUtils::finishContextInsensitiveAnalysis();
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
