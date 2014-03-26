#include "Analysis/InfoFlow/DeclassifierAnalysis.h"
#include "Util/ClassifiedUtils.h"
#include "Util/DebugUtils.h"
#include "soaap.h"

#include "llvm/IR/CFG.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IntrinsicInst.h"

using namespace soaap;

void DeclassifierAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {

  SDEBUG("soaap.analysis.infoflow.declassify", 3, dbgs() << "Starting declassifier analysis\n");

  // initialise with pointers to annotated fields and uses of annotated global variables
  string declassifyFuncBaseName = "__soaap_declassify";
  for (Function& F : M.getFunctionList()) {
    if (F.getName().startswith(declassifyFuncBaseName)) {
      SDEBUG("soaap.analysis.infoflow.declassify", 3, dbgs() << "   Found " << F.getName() << " function\n");
      for (User* U : F.users()) {
        if (CallInst* call = dyn_cast<CallInst>(U)) {
          //call->dump();
          if (LoadInst* declassifiedLoad = dyn_cast<LoadInst>(call->getArgOperand(0)->stripPointerCasts())) {
            // at the moment we only handle allocas and not fields
            if (AllocaInst* alloca = dyn_cast<AllocaInst>(declassifiedLoad->getPointerOperand())) {
              // collect all instructions starting from the annotation to the end of the current function
              findAllFollowingInstructions(call, alloca);
              // mark all loaded values of alloca within the declassified region
              // as being declassified (this will then propagate throughout the 
              // program)
              SDEBUG("soaap.analysis.infoflow.declassify", 3, dbgs() << "Declassified code region\n");
              for (Instruction* I : valueToDeclassifiedRegion[alloca]) {
                SDEBUG("soaap.analysis.infoflow.declassify", 3, dbgs() << "I: " << *I << "\n");
                if (LoadInst* L = dyn_cast<LoadInst>(I)) {
                  if (L->getPointerOperand() == alloca) {
                    SDEBUG("soaap.analysis.infoflow.declassify", 3, dbgs() << "Adding " << *L << " to worklist\n");
                    state[ContextUtils::NO_CONTEXT][L] = true;
                    addToWorklist(L, ContextUtils::NO_CONTEXT, worklist);
                  }
                }
              }

            }
            else {
              outs() << "SOAAP ERROR: Only declassification of local variables/function arguments is currently supported\n";
            }
          }
        }
      }
    }
  }
}

void DeclassifierAnalysis::findAllFollowingInstructions(Instruction* I, Value* V) {
  BasicBlock* BB = I->getParent();
  
  // If I is not the start of BB, then fast-forward iterator to it
  BasicBlock::iterator BI = BB->begin();
  while (&*BI != I) { BI++; }

  for (BasicBlock::iterator BE = BB->end(); BI != BE; BI++) {
    I = &*BI;
    SDEBUG("soaap.analysis.infoflow.declassify", 3, I->dump());
    valueToDeclassifiedRegion[V].push_back(I);
  }

  // recurse on successor BBs
  for (succ_iterator SI = succ_begin(BB), SE = succ_end(BB); SI != SE; SI++) {
    BasicBlock* SBB = *SI;
    findAllFollowingInstructions(SBB->begin(), V);
  }
}

void DeclassifierAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  SDEBUG("soaap.analysis.infoflow.declassify", 3, dbgs() << "Finished declassifier analysis\n");
}

bool DeclassifierAnalysis::performMeet(bool from, bool& to) {
  bool oldTo = to;
  to = from && to;
  return to != oldTo;
}

bool DeclassifierAnalysis::isDeclassified(const Value* V) {
  return state[ContextUtils::SINGLE_CONTEXT][V];
}

string DeclassifierAnalysis::stringifyFact(bool fact) {
  return fact ? "true" : "false";
}
