#include "llvm/DebugInfo.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"
#include "Analysis/InfoFlow/AccessOriginAnalysis.h"
#include "Utils/LLVMAnalyses.h"
#include "Utils/PrettyPrinters.h"

using namespace soaap;

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
      list<Instruction*> trace1 = PrettyPrinters::findPathToFunc(MainFn, Via, NULL, -1);
      list<Instruction*> trace2 = PrettyPrinters::findPathToFunc(Via, Target, &state, ORIGIN_SANDBOX);
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
