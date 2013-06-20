#include "Util/CallGraphUtils.h"
#include "Util/LLVMAnalyses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/ProfileInfo.h"

using namespace soaap;
using namespace llvm;

void CallGraphUtils::loadDynamicCallGraphEdges(Module& M) {
  if (ProfileInfo* PI = LLVMAnalyses::getProfileInfoAnalysis()) {
    CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
    for (Module::iterator F1 = M.begin(), E1 = M.end(); F1 != E1; ++F1) {
      if (F1->isDeclaration()) continue;
      for (inst_iterator I = inst_begin(F1), E = inst_end(F1); I != E; ++I) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          C->dump();
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

