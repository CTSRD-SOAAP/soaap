#include "llvm/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Util/CallGraphUtils.h"
#include "Util/LLVMAnalyses.h"

using namespace soaap;
using namespace llvm;

void InfoFlowAnalysis::doAnalysis(Module& M, SandboxVector& sandboxes) {
  ValueList worklist;
  initialise(worklist, M, sandboxes);
  performDataFlowAnalysis(worklist, M);
  postDataFlowAnalysis(M, sandboxes);
}

void InfoFlowAnalysis::performDataFlowAnalysis(ValueList& worklist, Module& M) {
  // perform propagation until fixed point is reached
  while (!worklist.empty()) {
    const Value* V = worklist.front();
    worklist.pop_front();
    DEBUG(outs() << "*** Popped " << V->getName() << "\n");
    DEBUG(V->dump());
    for (Value::const_use_iterator VI=V->use_begin(), VE=V->use_end(); VI != VE; VI++) {
      const Value* V2;
      DEBUG(VI->dump());
      if (const StoreInst* SI = dyn_cast<const StoreInst>(*VI)) {
        if (V == SI->getPointerOperand()) // to avoid infinite looping
          continue;
        V2 = SI->getPointerOperand();
      }
      else if (const CallInst* CI = dyn_cast<const CallInst>(*VI)) {
        // propagate to the callee(s)
        DEBUG(dbgs() << "Call instruction; propagating to callees\n");
        propagateToCallees(CI, V, worklist, M);
        continue;
      }
      else if (const ReturnInst* RI = dyn_cast<const ReturnInst>(*VI)) {
        if (Value* RetVal = RI->getReturnValue()) {
          DEBUG(dbgs() << "Return instruction; propagating to callers\n");
          DEBUG(RI->dump());
          propagateToCallers(RI, RetVal, worklist, M);
        }
        continue;
      }
      else {
        V2 = *VI;
      }
      if (propagateToValue(V, V2, M)) { // propagate taint from V to V2
        if (find(worklist.begin(), worklist.end(), V2) == worklist.end()) {
          worklist.push_back(V2);
        }
      }
      DEBUG(outs() << "Propagating " << V->getName() << " to " << V2->getName() << "\n");
    }
  }
}

bool InfoFlowAnalysis::propagateToValue(const Value* from, const Value* to, Module& M) {
  if (state.find(to) == state.end()) {
    state[to] = state[from];
    return true; // return true to allow sandbox names to propagate through
                 // regardless of whether the value was non-zero
  }                   
  else {
    int old = state[to];
    state[to] = performMeet(state[from], state[to]);
    return state[to] != old;
  }
}

void InfoFlowAnalysis::propagateToCallees(const CallInst* CI, const Value* V, ValueList& worklist, Module& M) {
  for (const Function* callee : CallGraphUtils::getCallees(CI, M)) {
    DEBUG(dbgs() << "Propagating to callee " << callee->getName() << "\n");
    // NOTE: no way to index a function's list of parameters
    int argIdx = 0;
    for (Function::const_arg_iterator AI=callee->arg_begin(), AE=callee->arg_end(); AI!=AE; AI++, argIdx++) {
      if (CI->getArgOperand(argIdx)->stripPointerCasts() == V) {
        if (propagateToValue(V, AI, M)) { // propagate 
          if (find(worklist.begin(), worklist.end(), AI) == worklist.end()) {
            worklist.push_back(AI);
          }
        }
      }
    }
  }
}

void InfoFlowAnalysis::propagateToCallers(const ReturnInst* RI, const Value* V, ValueList& worklist, Module& M) {
  const Function* F = RI->getParent()->getParent();
  for (const CallInst* C : CallGraphUtils::getCallers(F, M)) {
    if (propagateToValue(V, C, M)) {
      if (find(worklist.begin(), worklist.end(), C) == worklist.end()) {
        worklist.push_back(C);
      }
    }
  }
}


int InfoFlowAnalysis::performMeet(int from, int to) {
  return from | to;
}
