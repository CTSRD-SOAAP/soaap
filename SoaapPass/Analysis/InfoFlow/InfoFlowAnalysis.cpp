#include "llvm/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Util/CallGraphUtils.h"
#include "Util/ContextUtils.h"
#include "Util/DebugUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/SandboxUtils.h"

using namespace soaap;
using namespace llvm;

Context* const InfoFlowAnalysis::NO_CONTEXT = new Context();
Context* const InfoFlowAnalysis::PRIV_CONTEXT = new Context();
Context* const InfoFlowAnalysis::SINGLE_CONTEXT = new Context();

void InfoFlowAnalysis::doAnalysis(Module& M, SandboxVector& sandboxes) {
  ValueContextPairList worklist;
  initialise(worklist, M, sandboxes);
  performDataFlowAnalysis(worklist, sandboxes, M);
  postDataFlowAnalysis(M, sandboxes);
}

void InfoFlowAnalysis::performDataFlowAnalysis(ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M) {

  // merge contexts if this is a context-insensitive analysis
  if (ContextUtils::IsContextInsensitiveAnalysis) {
    DEBUG(dbgs() << INDENT_1 << "Merging contexts\n");
    worklist.clear();
    for (map<Context*,DataflowFacts>::iterator I=state.begin(), E=state.end(); I != E; I++) {
      Context* C = I->first;
      DataflowFacts F = I->second;
      for (DataflowFacts::iterator DI=F.begin(), DE=F.end(); DI != DE; DI++) {
        const Value* V = DI->first;
        int T = DI->second;
        state[SINGLE_CONTEXT][V] = T;
        addToWorklist(V, SINGLE_CONTEXT, worklist);
      }
    }
  }

  // perform propagation until fixed point is reached
  while (!worklist.empty()) {
    ValueContextPair P = worklist.front();
    const Value* V = P.first;
    Context* C = P.second;
    worklist.pop_front();

    DEBUG(dbgs() << INDENT_1 << "Popped " << V->getName() << ", context: " << ContextUtils::stringifyContext(C) << ", value dump: "; V->dump(););
    DEBUG(dbgs() << INDENT_2 << "Finding uses\n");
    for (Value::const_use_iterator UI=V->use_begin(), UE=V->use_end(); UI != UE; UI++) {
      User* U = dyn_cast<User>(UI.getUse().getUser());
      DEBUG(dbgs() << INDENT_3 << "Use: "; U->dump(););
      const Value* V2;
      if (Constant* CS = dyn_cast<Constant>(U)) {
        V2 = CS;
        if (propagateToValue(V, V2, C, C, M)) { // propagate taint from (V,C) to (V2,C)
          DEBUG(dbgs() << INDENT_4 << "Propagating ("; V->dump(););
          DEBUG(dbgs() << ", " << ContextUtils::stringifyContext(C) << ") to ("; V2->dump(););
          DEBUG(dbgs() << ", " << ContextUtils::stringifyContext(C) << ")\n");
          addToWorklist(V2, C, worklist);
        }
      }
      else if (Instruction* I = dyn_cast<Instruction>(UI.getUse().getUser())) {
        if (C == NO_CONTEXT) {
          // update the taint value for the correct context and put the new pair on the worklist
          ContextVector C2s = ContextUtils::getContextsForMethod(I->getParent()->getParent(), sandboxes, M);
          for (Context* C2 : C2s) {
            propagateToValue(V, V, C, C2, M); 
            addToWorklist(V, C2, worklist);
          }
        }
        else if (ContextUtils::isInContext(I, C, sandboxes, M)) { // check if using instruction is in context C
          if (StoreInst* SI = dyn_cast<StoreInst>(I)) {
            if (V == SI->getPointerOperand()) // to avoid infinite looping
              continue;
            V2 = SI->getPointerOperand();
          }
          else if (CallInst* CI = dyn_cast<CallInst>(I)) {
            // propagate to the callee(s)
            DEBUG(dbgs() << INDENT_4 << "Call instruction; propagating to callees\n");
            propagateToCallees(CI, V, C, worklist, sandboxes, M);
            continue;
          }
          else if (ReturnInst* RI = dyn_cast<ReturnInst>(I)) {
            if (Value* RetVal = RI->getReturnValue()) {
              DEBUG(dbgs() << INDENT_4 << "Return instruction; propagating to callers\n");
              propagateToCallers(RI, RetVal, C, worklist, sandboxes, M);
            }
            continue;
          }
          else {
            V2 = I;
          }
          if (propagateToValue(V, V2, C, C, M)) { // propagate taint from (V,C) to (V2,C)
            DEBUG(dbgs() << INDENT_4 << "Propagating ("; V->dump(););
            DEBUG(dbgs() << ", " << ContextUtils::stringifyContext(C) << ") to ("; V2->dump(););
            DEBUG(dbgs() << ", " << ContextUtils::stringifyContext(C) << ")\n");
            addToWorklist(V2, C, worklist);
          }
        }
      }
    }
  }

  // unmerge contexts if this is a context-insensitive analysis
  if (ContextUtils::IsContextInsensitiveAnalysis) {
    DEBUG(dbgs() << INDENT_1 << "Unmerging contexts\n");
    ContextVector Cs = ContextUtils::getAllContexts(sandboxes);
    DataflowFacts F = state[SINGLE_CONTEXT];
    for (DataflowFacts::iterator I=F.begin(), E=F.end(); I != E; I++) {
      const Value* V = I->first;
      int T = I->second;
      for (Context* C : Cs) {
        state[C][V] = T;
      }
    }
  }

}

void InfoFlowAnalysis::addToWorklist(const Value* V, Context* C, ValueContextPairList& worklist) {
  ValueContextPair P = make_pair(V, C);
  if (find(worklist.begin(), worklist.end(), P) == worklist.end()) {
    worklist.push_back(P);
  }
}

bool InfoFlowAnalysis::propagateToValue(const Value* from, const Value* to, Context* cFrom, Context* cTo, Module& M) {
  if (state[cTo].find(to) == state[cTo].end()) {
    state[cTo][to] = state[cFrom][from];
    return true; // return true to allow sandbox names to propagate through
                 // regardless of whether the value was non-zero
  }                   
  else {
    int old = state[cTo][to];
    state[cTo][to] = performMeet(state[cFrom][from], old);
    return state[cTo][to] != old;
  }
}

int InfoFlowAnalysis::performMeet(int from, int to) {
  return from | to;
}

void InfoFlowAnalysis::propagateToCallees(CallInst* CI, const Value* V, Context* C, ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M) {
  for (Function* callee : CallGraphUtils::getCallees(CI, M)) {
    DEBUG(dbgs() << INDENT_5 << "Propagating to callee " << callee->getName() << "\n");
    Context* C2 = ContextUtils::calleeContext(C, callee, sandboxes, M);
    // NOTE: no way to index a function's list of parameters
    int argIdx = 0;
    for (Function::const_arg_iterator AI=callee->arg_begin(), AE=callee->arg_end(); AI!=AE; AI++, argIdx++) {
      if (CI->getArgOperand(argIdx)->stripPointerCasts() == V) {
        if (propagateToValue(V, AI, C, C2, M)) { // propagate 
          addToWorklist(AI, C2, worklist);
        }
      }
    }
  }
}

void InfoFlowAnalysis::propagateToCallers(ReturnInst* RI, const Value* V, Context* C, ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M) {
  Function* F = RI->getParent()->getParent();
  for (CallInst* CI : CallGraphUtils::getCallers(F, M)) {
    DEBUG(dbgs() << INDENT_5 << "Propagating to caller "; CI->dump(););
    ContextVector C2s = ContextUtils::callerContexts(RI, CI, C, sandboxes, M);
    for (Context* C2 : C2s) {
      if (propagateToValue(V, CI, C, C2, M)) {
        addToWorklist(CI, C2, worklist);
      }
    }
  }
}
