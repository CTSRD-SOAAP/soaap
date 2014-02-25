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
      InfoFlowAnalysis(bool contextInsens = false) : contextInsensitive(contextInsens) { }
      virtual void doAnalysis(Module& M, SandboxVector& sandboxes);

    protected:
      map<Context*, DataflowFacts> state;
      bool contextInsensitive;
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) = 0;
      virtual void performDataFlowAnalysis(ValueContextPairList&, SandboxVector& sandboxes, Module& M);
      // performMeet: toVal = fromVal /\ toVal. return true <-> toVal != fromVal /\ toVal
      virtual bool performMeet(FactType fromVal, FactType& toVal) = 0;
      virtual bool propagateToValue(const Value* from, const Value* to, Context* cFrom, Context* cTo, Module& M);
      virtual void propagateToCallees(CallInst* CI, const Value* V, Context* C, bool propagateAllArgs, ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M);
      virtual void propagateToCallers(ReturnInst* RI, const Value* V, Context* C, ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M);
      virtual Value* propagateForExternCall(CallInst* CI, const Value* V);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) = 0;
      virtual void addToWorklist(const Value* V, Context* C, ValueContextPairList& worklist);
      virtual FactType bottomValue() = 0;
      virtual string stringifyFact(FactType f) = 0;
      virtual string stringifyValue(const Value* V);
      virtual void stateChangedForFunctionPointer(CallInst* CI, const Value* FP, FactType& newState);
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
    if (contextInsensitive) {
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

      DEBUG(dbgs() << INDENT_1 << "Popped " << V->getName() << ", context: " << ContextUtils::stringifyContext(C) << ", value dump: " << stringifyValue(V) << "\n");
      DEBUG(dbgs() << "state[C][V]: " << stringifyFact(state[C][V]) << "\n");
      
      // special case for GEP (propagate to the aggregate)
      if (GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>((Value*)V)) {
        DEBUG(dbgs() << INDENT_2 << "GEP\n");
        // rewind to the aggregate and propagate
        Value* Agg = GEP->getPointerOperand()->stripInBoundsOffsets();
        while (!(isa<AllocaInst>(Agg) || isa<GlobalVariable>(Agg))) {
          if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(Agg)) {
            Agg = gep->getPointerOperand();
          }
          else if (LoadInst* load = dyn_cast<LoadInst>(Agg)) {
            Agg = load->getPointerOperand();
          }
          else if (CallInst* call = dyn_cast<CallInst>(Agg)) {
            // in this case, we need to rewind back to the local/global Value*
            // that flows to the return value of call's callee. This would
            // involve doing more information flow analysis. However, for now
            // we hardcode the specific cases:
            //TODO: replace!
            if (call->getCalledFunction()->getName() == "buffer_ptr") {
              Agg = call->getArgOperand(0);
              DEBUG(Agg->dump());
            }
            else {
              dbgs() << "WARNING: unexpected instruction: " << *Agg << "\n";
            }
          }
          else {
            dbgs() << "WARNING: unexpected instruction: " << *Agg << "\n";
          }
          Agg = Agg->stripInBoundsOffsets();
        }
        if (propagateToValue(V, Agg, C, C, M)) { 
          DEBUG(dbgs() << INDENT_3 << "propagating to aggregate\n");
          addToWorklist(Agg, C, worklist);
        }
      }

      DEBUG(dbgs() << INDENT_2 << "Finding uses (" << V->getNumUses() << ")\n");
      for (Value::const_use_iterator UI=V->use_begin(), UE=V->use_end(); UI != UE; UI++) {
        User* U = dyn_cast<User>(UI.getUse().getUser());
        DEBUG(dbgs() << INDENT_3 << "Use: " << stringifyValue(U) << "\n";);
        const Value* V2 = NULL;
        if (Constant* CS = dyn_cast<Constant>(U)) {
          V2 = CS;
          if (propagateToValue(V, V2, C, C, M)) { // propagate taint from (V,C) to (V2,C)
            DEBUG(dbgs() << INDENT_4 << "Propagating (" << stringifyValue(V));
            DEBUG(dbgs() << ", " << ContextUtils::stringifyContext(C) << ") to (" << stringifyValue(V2) << "\n";);
            DEBUG(dbgs() << ", " << ContextUtils::stringifyContext(C) << ")\n");
            addToWorklist(V2, C, worklist);
          }
        }
        else if (Instruction* I = dyn_cast<Instruction>(UI.getUse().getUser())) {
          if (C == ContextUtils::NO_CONTEXT) {
            // update the taint value for the correct context and put the new pair on the worklist
            ContextVector C2s = ContextUtils::getContextsForMethod(I->getParent()->getParent(), contextInsensitive, sandboxes, M);
            for (Context* C2 : C2s) {
              DEBUG(dbgs() << INDENT_4 << "Propagating (" << stringifyValue(V));
              DEBUG(dbgs() << ", " << ContextUtils::stringifyContext(C) << ") to (" << stringifyValue(V));
              DEBUG(dbgs() << ", " << ContextUtils::stringifyContext(C2) << ")\n");
              propagateToValue(V, V, C, C2, M); 
              addToWorklist(V, C2, worklist);
            }
          }
          else if (ContextUtils::isInContext(I, C, contextInsensitive, sandboxes, M)) { // check if using instruction is in context C
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
                V2 = propagateForExternCall(CI, V);
              }
              else {
                bool propagateAllArgs = false;
                if (CI->getCalledValue()->stripPointerCasts() == V) {
                  // subclasses might want to be informed when
                  // the state of a function pointer changed
                  stateChangedForFunctionPointer(CI, V, state[C][V]);
                  
                  // if callee information has changed, we should propagate all
                  // args to callees in case this is the first time for some
                  propagateAllArgs = true;
                }
                propagateToCallees(CI, V, C, propagateAllArgs, worklist, sandboxes, M);
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
            else if (PHINode* PHI = dyn_cast<PHINode>(I)) {
              // take the meet of all incoming values
              bool change = false;
              int i;
              for (i=0; i<PHI->getNumIncomingValues(); i++) {
                Value* IV = PHI->getIncomingValue(i);
                if (propagateToValue(IV, PHI, C, C, M)) {
                  change = true;
                }
              }
              if (change) {
                addToWorklist(PHI, C, worklist);
              }
            }
            else {
              V2 = I; // this covers gep instructions
            }
            if (V2 != NULL && propagateToValue(V, V2, C, C, M)) { // propagate taint from (V,C) to (V2,C)
              DEBUG(dbgs() << INDENT_4 << "Propagating (" << stringifyValue(V));
              DEBUG(dbgs() << ", " << ContextUtils::stringifyContext(C) << ") to (" << stringifyValue(V2));
              DEBUG(dbgs() << ", " << ContextUtils::stringifyContext(C) << ")\n");
              addToWorklist(V2, C, worklist);
            }
          }
        }
      }
    }

    // unmerge contexts if this is a context-insensitive analysis
    if (contextInsensitive) {
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
      //FactType old = state[cTo][to];
      //state[cTo][to] = performMeet(state[cFrom][from], old);
      return performMeet(state[cFrom][from], state[cTo][to]);
    }
  }


  template <typename FactType>
  void InfoFlowAnalysis<FactType>::propagateToCallees(CallInst* CI, const Value* V, Context* C, bool propagateAllArgs, ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M) {

    DEBUG(dbgs() << "Call instruction: " << *CI << "\n");
    DEBUG(dbgs() << "Calling-context C: " << ContextUtils::stringifyContext(C) << "\n");

    for (Function* callee : CallGraphUtils::getCallees(CI, M)) {
      DEBUG(dbgs() << INDENT_5 << "Propagating to callee " << callee->getName() << "\n");
      Context* C2 = ContextUtils::calleeContext(C, contextInsensitive, callee, sandboxes, M);
      DEBUG(dbgs() << INDENT_6 << "Callee-context C2: " << ContextUtils::stringifyContext(C2) << "\n");
      Function::ArgumentListType& params = callee->getArgumentList();

      // To be sound, we need to take the meet of all values passed in for each
      // parameter that we are propagating to (i.e. from all other call sites and
      // not only CI). Otherwise, must analyses will lead to incorrect results.
      CallInstSet callers = CallGraphUtils::getCallers(callee, M);

      // NOTE: no way to index a function's list of parameters
      // NOTE2: If this is a variadic parmaeter, then propagate to callee's va_list
      int argIdx = 0;
      for (Function::const_arg_iterator AI=callee->arg_begin(), AE=callee->arg_end(); AI!=AE; AI++, argIdx++) {
        if (propagateAllArgs || CI->getArgOperand(argIdx)->stripPointerCasts() == V) {
          DEBUG(dbgs() << INDENT_6 << "Propagating to " << stringifyValue(AI));
          // Take meet of all argument values passed in at argIdx by 
          // all callers in context C. This makes our analysis sound
          // when our meet operator is intersection.
          DEBUG(dbgs() << INDENT_6 << "Taking meet of all arg idx " << argIdx << " values from all callers\n");
          bool change = false;
          for (CallInst* caller : callers) { // CI will be in callers
            if (ContextUtils::isInContext(caller, C, contextInsensitive, sandboxes, M)) {
              Value* V2 = caller->getArgOperand(argIdx)->stripPointerCasts();
              if (propagateToValue(V2, AI, C, C2, M)) { // propagate
                change = true;
              }
            }
          }
          if (change) {
            DEBUG(dbgs() << "Adding (AI,C2) to worklist\n");
            DEBUG(dbgs() << "state[C2][AI]: " << stringifyFact(state[C2][AI]) << "\n");
            addToWorklist(AI, C2, worklist);
          }
        }
      }
      // check var args (if any)
      if (callee->isVarArg()) {
        // find va_list var, it will have type [1 x %struct.__va_list_tag]*
        BasicBlock& EntryBB = callee->getEntryBlock();
        Value* VarArgPtr = NULL;
        
        for (BasicBlock::iterator I = EntryBB.begin(), E = EntryBB.end(); I != E; I++) {
          if (AllocaInst* Alloca = dyn_cast<AllocaInst>(I)) {
            if (ArrayType* AT = dyn_cast<ArrayType>(Alloca->getAllocatedType())) {
              if (StructType* ST = dyn_cast<StructType>(AT->getElementType())) {
                DEBUG(dbgs() << "Struct type has name: " << ST->getName() << "\n");
                if (ST->getName() == "struct.__va_list_tag") {
                  VarArgPtr = Alloca;
                  break;
                }
              }
            }
          }
        }
        if (VarArgPtr == NULL) {
          dbgs() << "SOAAP ERROR: Could not find var arg pointer in " << callee->getName() << "\n";
        }
        else {
          for (; argIdx < CI->getNumArgOperands(); argIdx++) {
            if (CI->getArgOperand(argIdx)->stripPointerCasts() == V) {
              DEBUG(dbgs() << INDENT_6 << "Propagating to VarArgPtr");
              if (propagateToValue(V, VarArgPtr, C, C2, M)) { // propagate
                DEBUG(dbgs() << "Adding VarArgPtr to worklist: " << stringifyValue(VarArgPtr));
                addToWorklist(VarArgPtr, C2, worklist);
              }
            }
          }
        }
      }
    }
  }

  template <typename FactType>
  void InfoFlowAnalysis<FactType>::propagateToCallers(ReturnInst* RI, const Value* V, Context* C, ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M) {
    Function* F = RI->getParent()->getParent();
    for (CallInst* CI : CallGraphUtils::getCallers(F, M)) {
      DEBUG(dbgs() << INDENT_5 << "Propagating to caller " << stringifyValue(CI));
      ContextVector C2s = ContextUtils::callerContexts(RI, CI, C, contextInsensitive, sandboxes, M);
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
    string funcName = F->getName();
    if (funcName == "strdup") {
      return CI;
    }
    else if (funcName == "asprintf" || funcName == "vasprintf") { 
      // if V is not the format string or the out param, then propagate to out param
      if (CI->getArgOperand(0) != V && CI->getArgOperand(1) != V) {
        return CI->getArgOperand(0);
      }
    }
    else if (funcName == "strcpy") {
      if (CI->getArgOperand(0) != V) { // V is not dst, so it must be src
        return CI->getArgOperand(0);
      }
    }
    DEBUG(dbgs() << "Returning NULL\n");
    return NULL;
  }

  template <typename FactType>
  string InfoFlowAnalysis<FactType>::stringifyValue(const Value* V) {
    string result;
    raw_string_ostream ss(result);
    if (isa<Function>(V)) {
      ss << V->getName();
    }
    else {
      V->print(ss);
    }
    ss << " - " << *V->getType();
    return result;
  }

  // default behaviour is to do nothing
  template<typename FactType>
  void InfoFlowAnalysis<FactType>::stateChangedForFunctionPointer(CallInst* CI, const Value* FP, FactType& newState) {
  }

}

#endif
