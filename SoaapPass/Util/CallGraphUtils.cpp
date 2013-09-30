#include "Analysis/InfoFlow/FPTargetsAnalysis.h"
#include "Util/CallGraphUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/ProfileInfo.h"

#include <sstream>

#include "soaap.h"

using namespace soaap;
using namespace llvm;

map<const CallInst*, FunctionVector> CallGraphUtils::callToCallees;
map<const Function*, CallInstVector> CallGraphUtils::calleeToCalls;
FPTargetsAnalysis CallGraphUtils::fpTargetsAnalysis;
bool CallGraphUtils::caching = false;

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

void CallGraphUtils::listFPCalls(Module& M) {
  CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  unsigned long numFPcalls = 0;
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    bool displayedFuncName = false;
    CallGraphNode* callerNode = CG->getOrInsertFunction(F);
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (!isa<IntrinsicInst>(&*I)) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          if (C->getCalledFunction() == NULL) {
            if (MDNode* N = C->getMetadata("dbg")) {
              DILocation loc(N);
              if (!displayedFuncName) {
                // only display function on first function-pointer call
                outs() << INDENT_1 << F->getName() << ":\n";
                displayedFuncName = true;
              }
              outs() << INDENT_2 << "Call: " << loc.getFilename().str() << ":" << loc.getLineNumber() << "\n";
            }
            numFPcalls++;
          }
        }
      }
    }
  }
  outs() << numFPcalls << " function-pointer calls in total\n";
}

void CallGraphUtils::loadAnnotatedCallGraphEdges(Module& M) {
  // Find annotated function pointers and add edges from the calls of the fp to targets.
  // Because annotated pointers can be assigned and passed around, we essentially perform
  // an information flow analysis:
  SandboxVector dummyVector;
  fpTargetsAnalysis.doAnalysis(M, dummyVector);

  // for each fp-call, add annotated edges to the call graph
  DEBUG(dbgs() << INDENT_1 << "Finding all fp calls\n");
  CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    CallGraphNode* callerNode = CG->getOrInsertFunction(F);
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (!isa<IntrinsicInst>(&*I)) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          if (C->getCalledFunction() == NULL) {
            Value* FP = C->getCalledValue();
            DEBUG(dbgs() << INDENT_2 << "Caller: " << C->getParent()->getParent()->getName() << "\n");
            DEBUG(dbgs() << INDENT_2 << "Found fp call: " << *C << "\n");
            DEBUG(dbgs() << INDENT_3 << "FP: " << *FP << "\n");
            DEBUG(dbgs() << INDENT_3 << "Targets: ";);
            for (Function* T : fpTargetsAnalysis.getTargets(FP)) {
              DEBUG(dbgs() << " " << T->getName());
              CallGraphNode* calleeNode = CG->getOrInsertFunction(T);
              callerNode->addCalledFunction(CallSite(C), calleeNode);
            }
            DEBUG(dbgs() << "\n");
          }
        }
      }
    }
  }

  // repopulate caches, because they would've been populated already for the FPTargetsAnalysis
  // and now turn on caching so future calls to getCallees and getCallers read from the caches.
  populateCallCalleeCaches(M);
  caching = true;
}

FunctionVector CallGraphUtils::getCallees(const CallInst* C, Module& M) {
  DEBUG(dbgs() << INDENT_5 << "Getting callees for " << *C << "\n");
  if (callToCallees.empty() || !caching) {
    populateCallCalleeCaches(M);
  }
  DEBUG(dbgs() << INDENT_5 << "Callees: ");
  for (Function* F : callToCallees[C]) {
    DEBUG(dbgs() << F->getName() << " ");
  }
  DEBUG(dbgs() << "\n");
  return callToCallees[C];
}

CallInstVector CallGraphUtils::getCallers(const Function* F, Module& M) {
  DEBUG(dbgs() << INDENT_5 << "Getting callers for " << F->getName() << "\n");
  if (calleeToCalls.empty() || !caching) {
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
        else if (Value* FP = C->getCalledValue())  { // dynamic/annotated callees
          if (ProfileInfo* PI = LLVMAnalyses::getProfileInfoAnalysis()) {
            for (const Function* callee : PI->getDynamicCallees(C)) {
              DEBUG(dbgs() << INDENT_3 << "Adding dyn-callee " << callee->getName() << "\n");
              callees.push_back((Function*)callee);
            }
          }
          for (Function* callee : fpTargetsAnalysis.getTargets(FP)) {
            DEBUG(dbgs() << INDENT_3 << "Adding fp-callee " << callee->getName() << "\n");
            callees.push_back(callee);
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
