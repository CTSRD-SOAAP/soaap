#include "Util/CallGraphUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/DebugUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/ProfileInfo.h"

using namespace soaap;
using namespace llvm;

map<const CallInst*, FunctionVector> CallGraphUtils::callToCallees;
map<const Function*, CallInstVector> CallGraphUtils::calleeToCalls;

void CallGraphUtils::loadDynamicCallGraphEdges(Module& M) {
  if (ProfileInfo* PI = LLVMAnalyses::getProfileInfoAnalysis()) {
    CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
    for (Module::iterator F1 = M.begin(), E1 = M.end(); F1 != E1; ++F1) {
      if (F1->isDeclaration()) continue;
      for (inst_iterator I = inst_begin(F1), E = inst_end(F1); I != E; ++I) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          for (const Function* F2 : PI->getDynamicCallees(C)) {
            DEBUG(dbgs() << "F2: " << F2->getName() << "\n");
            CallGraphNode* F1Node = CG->getOrInsertFunction(F1);
            CallGraphNode* F2Node = CG->getOrInsertFunction(F2);
            DEBUG(dbgs() << "loadDynamicCallEdges: adding " << F1->getName() << " -> " << F2->getName() << "\n");
            F1Node->addCalledFunction(CallSite(C), F2Node);
          }
        }
      }
    }
  }
}

FunctionVector CallGraphUtils::getCallees(const CallInst* C, Module& M) {
  DEBUG(dbgs() << INDENT_5 << "Getting callees for "; C->dump(););
  if (callToCallees.find(C) == callToCallees.end()) {
    populateCallCalleeCaches(M);
  }
  return callToCallees[C];
}

CallInstVector CallGraphUtils::getCallers(const Function* F, Module& M) {
  DEBUG(dbgs() << INDENT_5 << "Getting callers for " << F->getName() << "\n");
  if (calleeToCalls.find(F) == calleeToCalls.end()) {
    populateCallCalleeCaches(M);
  }
  return calleeToCalls[F];
}

void CallGraphUtils::populateCallCalleeCaches(Module& M) {
  DEBUG(dbgs() << "-----------------------------------------------------------------\n");
  DEBUG(dbgs() << INDENT_1 << "Populating call -> callees and callee -> calls cache\n");
  for (Module::iterator F1 = M.begin(), E1 = M.end(); F1 != E1; ++F1) {
    if (F1->isDeclaration()) continue;
    DEBUG(dbgs() << INDENT_2 << "Processing " << F1->getName() << "\n");
    for (inst_iterator I = inst_begin(F1), E = inst_end(F1); I != E; ++I) {
      if (CallInst* C = dyn_cast<CallInst>(&*I)) {
        FunctionVector callees;
        if (Function* callee = C->getCalledFunction()) {
          if (!callee->isIntrinsic()) {
            DEBUG(dbgs() << INDENT_3 << "Adding callee " << callee->getName() << "\n");
            callees.push_back(callee);
          }
        }
        else if (Value* FP = C->getCalledValue())  { // dynamic callees
          ProfileInfo* PI = LLVMAnalyses::getProfileInfoAnalysis();
          for (const Function* callee : PI->getDynamicCallees(C)) {
            DEBUG(dbgs() << INDENT_3 << "Adding callee " << callee->getName() << "\n");
            callees.push_back((Function*)callee);
          }
        }
        callToCallees[C] = callees;
        for (Function* callee : callees) {
          calleeToCalls[callee].push_back(C); // we process each C exactly once, so no dups!
        }
      }
    }
  }
  DEBUG(dbgs() << "-----------------------------------------------------------------\n");
}
