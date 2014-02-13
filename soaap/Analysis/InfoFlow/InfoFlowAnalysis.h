#ifndef SOAAP_ANALYSIS_INFOFLOW_INFOFLOWANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_INFOFLOWANALYSIS_H

#include <map>
#include <list>

#include "llvm/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Pass.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/Debug.h"

#include "Analysis/Analysis.h"
#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Util/CallGraphUtils.h"
#include "Util/ContextUtils.h"
#include "Util/DebugUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/SandboxUtils.h"

using namespace std;
using namespace llvm;

namespace soaap {
  // Base class for context-sensitive information-flow analysis.
  // There are three types of context: no context, privileged context and sandbox.
  // A fourth context "single" is used for context-insensitivity
  // These contexts are found in Context.h
  template<class FactType>
  class InfoFlowAnalysis : public Analysis {
    public:
      typedef map<const Value*, FactType> DataflowFacts;
      typedef pair<const Value*, Context*> ValueContextPair;
      typedef list<ValueContextPair> ValueContextPairList;
      virtual void doAnalysis(Module& M, SandboxVector& sandboxes);

    protected:
      map<Context*, DataflowFacts> state;
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) = 0;
      virtual void performDataFlowAnalysis(ValueContextPairList&, SandboxVector& sandboxes, Module& M);
      virtual FactType performMeet(FactType fromVal, FactType toVal) = 0;
      virtual bool propagateToValue(const Value* from, const Value* to, Context* cFrom, Context* cTo, Module& M);
      virtual void propagateToCallees(CallInst* CI, const Value* V, Context* C, ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M);
      virtual void propagateToCallers(ReturnInst* RI, const Value* V, Context* C, ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M);
      virtual Value* propagateForExternCall(CallInst* CI, const Value* V);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) = 0;
      virtual void addToWorklist(const Value* V, Context* C, ValueContextPairList& worklist);
      virtual FactType bottomValue() = 0;
  };

  template <class FactType>
  void InfoFlowAnalysis<FactType>::doAnalysis(Module& M, SandboxVector& sandboxes) {
    ValueContextPairList worklist;
    initialise(worklist, M, sandboxes);
    performDataFlowAnalysis(worklist, sandboxes, M);
    postDataFlowAnalysis(M, sandboxes);
  }

  template <typename FactType>
  void InfoFlowAnalysis<FactType>::performDataFlowAnalysis(ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M) {

    // merge contexts if this is a context-insensitive analysis
    if (ContextUtils::IsContextInsensitiveAnalysis) {
      DEBUG(dbgs() << INDENT_1 << "Merging contexts\n");
      worklist.clear();
      for (typename map<Context*,DataflowFacts>::iterator I=state.begin(), E=state.end(); I != E; I++) {
        Context* C = I->first;
        DataflowFacts F = I->second;
        for (typename DataflowFacts::iterator DI=F.begin(), DE=F.end(); DI != DE; DI++) {
          const Value* V = DI->first;
          FactType T = DI->second;
          state[ContextUtils::SINGLE_CONTEXT][V] = T; // TODO: what if same V appears in multiple contexts?
          addToWorklist(V, ContextUtils::SINGLE_CONTEXT, worklist);
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
      DEBUG(dbgs() << INDENT_2 << "Finding uses (" << V->getNumUses() << ")\n");
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
          if (C == ContextUtils::NO_CONTEXT) {
            // update the taint value for the correct context and put the new pair on the worklist
            ContextVector C2s = ContextUtils::getContextsForMethod(I->getParent()->getParent(), sandboxes, M);
            for (Context* C2 : C2s) {
              DEBUG(dbgs() << INDENT_4 << "Propagating ("; V->dump(););
              DEBUG(dbgs() << ", " << ContextUtils::stringifyContext(C) << ") to ("; V->dump(););
              DEBUG(dbgs() << ", " << ContextUtils::stringifyContext(C2) << ")\n");
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
            else if (IntrinsicInst* II = dyn_cast<IntrinsicInst>(I)) {
              if (II->getIntrinsicID() == Intrinsic::ptr_annotation) { // covers llvm.ptr.annotation.p0i8
                DEBUG(II->dump());
                V2 = II;
              }
            }
            else if (CallInst* CI = dyn_cast<CallInst>(I)) {
              // propagate to the callee(s)
              DEBUG(dbgs() << INDENT_4 << "Call instruction; propagating to callees\n");
              if (CallGraphUtils::isExternCall(CI)) { // no function body, so we approximate effects of known funcs
                if ((V2 = propagateForExternCall(CI, V)) == NULL) {
                  continue;
                }
              }
              else {
                propagateToCallees(CI, V, C, worklist, sandboxes, M);
                continue;
              }
            }
            else if (ReturnInst* RI = dyn_cast<ReturnInst>(I)) {
              if (Value* RetVal = RI->getReturnValue()) {
                DEBUG(dbgs() << INDENT_4 << "Return instruction; propagating to callers\n");
                propagateToCallers(RI, RetVal, C, worklist, sandboxes, M);
              }
              continue;
            }
            else if (I->isBinaryOp()) {
              // The resulting value is a combination of its operands and we do not combine
              // dataflow facts in this way. So we do not propagate the dataflow-value of V
              // but actually set it to the bottom value.
              DEBUG(dbgs() << INDENT_4 << "Binary operator, propagating bottom to " << *I << "\n");
              state[C][I] = bottomValue();
              addToWorklist(I, C, worklist);
              continue;
            }
            else {
              V2 = I; // this covers PHINode instructions
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
      DataflowFacts F = state[ContextUtils::SINGLE_CONTEXT];
      for (typename DataflowFacts::iterator I=F.begin(), E=F.end(); I != E; I++) {
        const Value* V = I->first;
        FactType T = I->second;
        for (Context* C : Cs) {
          state[C][V] = T;
        }
      }
    }

  }

  template <typename FactType>
  void InfoFlowAnalysis<FactType>::addToWorklist(const Value* V, Context* C, ValueContextPairList& worklist) {
    ValueContextPair P = make_pair(V, C);
    if (find(worklist.begin(), worklist.end(), P) == worklist.end()) {
      worklist.push_back(P);
    }
  }

  template <typename FactType>
  bool InfoFlowAnalysis<FactType>::propagateToValue(const Value* from, const Value* to, Context* cFrom, Context* cTo, Module& M) {
    if (state[cTo].find(to) == state[cTo].end()) {
      state[cTo][to] = state[cFrom][from];
      return true; // return true to allow sandbox names to propagate through
                   // regardless of whether the value was non-zero
    }                   
    else {
      FactType old = state[cTo][to];
      state[cTo][to] = performMeet(state[cFrom][from], old);
      return state[cTo][to] != old;
    }
  }


  template <typename FactType>
  void InfoFlowAnalysis<FactType>::propagateToCallees(CallInst* CI, const Value* V, Context* C, ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M) {
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

  template <typename FactType>
  void InfoFlowAnalysis<FactType>::propagateToCallers(ReturnInst* RI, const Value* V, Context* C, ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M) {
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

  template <typename FactType>
  Value* InfoFlowAnalysis<FactType>::propagateForExternCall(CallInst* CI, const Value* V) {
    // propagate dataflow value of relevant arg(s) (if happen to be V) to
    // the return value of the call CI
    Function* F = CI->getCalledFunction();
    DEBUG(dbgs() << "Extern call, f=" << F->getName() << "\n");
    if (F->getName() == "strdup") {
      return CI;
    }
    return NULL;
  }

}

#endif
