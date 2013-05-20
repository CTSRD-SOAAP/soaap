#include "llvm/DebugInfo.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"
#include "Analysis/InfoFlow/AccessOriginAnalysis.h"
#include "Utils/LLVMAnalyses.h"
#include "Utils/PrettyPrinters.h"

using namespace soaap;

bool AccessOriginAnalysis::findPathToFuncHelper(CallGraphNode* CurrNode, CallGraphNode* FinalNode, list<Instruction*>& trace, list<CallGraphNode*>& visited, bool useOrigins, int taint) {
  if (CurrNode == FinalNode)
    return true;
  else if (CurrNode->getFunction() == NULL) // non-function node (e.g. External node)
    return false;
  else if (find(visited.begin(), visited.end(), CurrNode) != visited.end()) // cycle
    return false;
  else {
    visited.push_back(CurrNode);
    for (CallGraphNode::iterator I = CurrNode->begin(), E = CurrNode->end(); I!=E; I++) {
      Value* V = I->first;
      if(CallInst* Call = dyn_cast_or_null<CallInst>(V)) {
        CallGraphNode* CalleeNode = I->second;
        bool proceed = true;
        if (useOrigins) {
          // check that Call has at least one tainted arg
          int idx;
          for (idx = 0; idx < Call->getNumArgOperands(); idx++) {
            if((proceed = (state[Call->getArgOperand(idx)->stripPointerCasts()] == taint))) {
              break;
            }
          }
        }
        if (proceed && findPathToFuncHelper(CalleeNode, FinalNode, trace, visited, useOrigins, taint)) {
          // CurrNode is on a path to FinalNode, so prepend to the trace
          trace.push_back(Call);
          return true;
        }
      }
    }
    return false;
  }
}

list<Instruction*> AccessOriginAnalysis::findPathToFunc(Function* From, Function* To, bool useOrigins, int taint) {
  CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  CallGraphNode* FromNode = (*CG)[From];
  CallGraphNode* ToNode = (*CG)[To];
  list<CallGraphNode*> visited;
  list<Instruction*> trace;
  findPathToFuncHelper(FromNode, ToNode, trace, visited, useOrigins, taint);
  return trace;
}

void AccessOriginAnalysis::ppPrivilegedPathToInstruction(Instruction* I, Module& M) {
  if (Function* MainFn = M.getFunction("main")) {
    // Find privileged path to instruction I, via a function that calls a sandboxed callee
    Function* Target = I->getParent()->getParent();
    CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
    CallGraphNode* TargetNode = (*CG)[Target];

    outs() << "  Possible causes\n";
    for (CallInst* C : untrustedSources) {
      Function* Via = C->getParent()->getParent();
      DEBUG(outs() << MainFn->getName() << " -> " << Via->getName() << " -> " << Target->getName() << "\n");
      list<Instruction*> trace1 = findPathToFunc(MainFn, Via, false, -1);
      list<Instruction*> trace2 = findPathToFunc(Via, Target, true, ORIGIN_SANDBOX);
      // check that we have successfully been able to find a full trace!
      if (!trace1.empty() && !trace2.empty()) {
        PrettyPrinters::ppTaintSource(C);
        // append target instruction I and trace1, to the end of trace2
        trace2.push_front(I);
        trace2.insert(trace2.end(), trace1.begin(), trace1.end());
        PrettyPrinters::ppTrace(trace2);
        outs() << "\n";
      }
    }

    outs() << "Unable to find a trace\n";
  }
}

void AccessOriginAnalysis::initialise(ValueList& worklist, Module& M) {
  for (Function* F : sandboxEntryPoints) {
    // find calls of F, if F actually returns something!
    if (!F->getReturnType()->isVoidTy()) {
      for (Value::use_iterator I=F->use_begin(), E=F->use_end(); I!=E; I++) {
        if (CallInst* C = dyn_cast<CallInst>(*I)) {
          worklist.push_back(C);
          state[C] = ORIGIN_SANDBOX;
          untrustedSources.push_back(C);
        }
      }
    }
  }
}

void AccessOriginAnalysis::postDataFlowAnalysis(Module& M) {
  // check that no untrusted function pointers are called in privileged methods
  for (Function* F : privilegedMethods) {
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I!=E; ++I) {
      if (CallInst* C = dyn_cast<CallInst>(&*I)) {
        if (C->getCalledFunction() == NULL) {
          if (state[C->getCalledValue()] == ORIGIN_SANDBOX) {
            Function* caller = cast<Function>(C->getParent()->getParent());
            outs() << " *** Untrusted function pointer call in " << caller->getName() << "\n";
            if (MDNode *N = C->getMetadata("dbg")) {  // Here I is an LLVM instruction
              DILocation loc(N);                      // DILocation is in DebugInfo.h
              unsigned line = loc.getLineNumber();
              StringRef file = loc.getFilename();
              StringRef dir = loc.getDirectory();
              outs() << " +++ Line " << line << " of file " << file << "\n";
            }
            outs() << "\n";
            ppPrivilegedPathToInstruction(C, M);
          }
        }
      }
    }
  }
}

