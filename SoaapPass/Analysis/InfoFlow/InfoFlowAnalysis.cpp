#include "llvm/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Utils/LLVMAnalyses.h"

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
        // propagate to the callee function's argument
        // find the index of the use position
        //outs() << "CALL: ";
        //CI->dump();
        //Function* Caller = const_cast<Function*>(CI->getParent()->getParent());
        //outs() << "CALLER: " << Caller->getName() << "\n";
        if (Function* callee = CI->getCalledFunction()) {
          propagateToCallee(CI, callee, worklist, V, M);
        }
        else if (const Value* FP = CI->getCalledValue())  { // dynamic callees
          ProfileInfo* PI = LLVMAnalyses::getProfileInfoAnalysis();
          DEBUG(dbgs() << "dynamic call edge propagation\n");
          DEBUG(CI->dump());
          list<const Function*> callees = PI->getDynamicCallees(CI);
          DEBUG(dbgs() << "number of dynamic callees: " << callees.size() << "\n");
          for (const Function* callee : callees) {
            DEBUG(dbgs() << "  " << callee->getName() << "\n");
            propagateToCallee(CI, callee, worklist, V, M);
          }
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

void InfoFlowAnalysis::propagateToCallee(const CallInst* CI, const Function* callee, ValueList& worklist, const Value* V, Module& M) {
  DEBUG(dbgs() << "Propagating to callee " << callee->getName() << "\n");
  int idx;
  for (idx = 0; idx < CI->getNumArgOperands(); idx++) {
    if (CI->getArgOperand(idx)->stripPointerCasts() == V) {
    // now find the parameter object. Annoyingly there is no way
    // of getting the Argument at a particular index, so...
      for (Function::const_arg_iterator AI=callee->arg_begin(), AE=callee->arg_end(); AI!=AE; AI++) {
      //outs() << "arg no: " << AI->getArgNo() << "\n";
        if (AI->getArgNo() == idx) {
          //outs() << "Pushing arg " << AI->getName() << "\n";
          if (find(worklist.begin(), worklist.end(), AI) == worklist.end()) {
            worklist.push_back(AI);
            propagateToValue(V, AI, M); // propagate 
          }
        }
      }
    }
  }
}

int InfoFlowAnalysis::performMeet(int from, int to) {
  return from | to;
}
