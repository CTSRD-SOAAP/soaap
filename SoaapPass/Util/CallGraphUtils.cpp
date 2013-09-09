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
map<CallInst*, FunctionVector> CallGraphUtils::fpCallToCallees;

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

void CallGraphUtils::loadAnnotatedCallGraphEdges(Module& M) {
  // find annotated function pointers and add edges from the calls of the fp to targets
  CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  if (Function* F = M.getFunction("llvm.var.annotation")) {
    map<Value*, FunctionVector> fpToTargets;
    for (User::use_iterator UI = F->use_begin(), UE = F->use_end(); UI != UE; UI++) {
      User* user = UI.getUse().getUser();
      if (IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(user)) {
        Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());
        GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
        ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
        StringRef annotationStrValStr = annotationStrValArray->getAsCString();
        if (annotationStrValStr.startswith(SOAAP_FP)) {
          FunctionVector callees;
          string funcListCsv = annotationStrValStr.substr(strlen(SOAAP_FP)+1); //+1 because of _
          DEBUG(dbgs() << INDENT_1 << "FP annotation " << annotationStrValStr << " found: " << *annotatedVar << ", funcList: " << funcListCsv << "\n");
          istringstream ss(funcListCsv);
          string func;
          while(getline(ss, func, ',')) {
            // trim leading and trailing spaces
            size_t start = func.find_first_not_of(" ");
            size_t end = func.find_last_not_of(" ");
            func = func.substr(start, end-start+1);
            DEBUG(dbgs() << INDENT_2 << "Function: " << func << "\n");
            if (Function* callee = M.getFunction(func)) {
              DEBUG(dbgs() << INDENT_3 << "Adding " << callee->getName() << "\n");
              callees.push_back(callee);
            }
          }
          fpToTargets[annotatedVar] = callees;
        }
      }
    }

    // for each fp-call, add annotated edges to the call graph
    DEBUG(dbgs() << INDENT_1 << "Finding all fp calls\n");
    for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
      if (F->isDeclaration()) continue;
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        if (!isa<IntrinsicInst>(&*I)) {
          if (CallInst* C = dyn_cast<CallInst>(&*I)) {
            if (C->getCalledFunction() == NULL) {
              Value* FP = C->getCalledValue();
              DEBUG(dbgs() << INDENT_2 << "Caller: " << C->getParent()->getParent()->getName() << "\n");
              DEBUG(dbgs() << INDENT_2 << "Found fp call: " << *C << "\n");
              if (LoadInst* L = dyn_cast<LoadInst>(FP)) {
                FP = L->getPointerOperand();
              }
              if (fpToTargets.find(FP) != fpToTargets.end()) {
                fpCallToCallees[C] = fpToTargets[FP];
              }
            }
          }
        }
      }
    }
  }
}

FunctionVector CallGraphUtils::getCallees(const CallInst* C, Module& M) {
  DEBUG(dbgs() << INDENT_5 << "Getting callees for " << *C << "\n");
  if (callToCallees.empty()) {
    populateCallCalleeCaches(M);
  }
  return callToCallees[C];
}

CallInstVector CallGraphUtils::getCallers(const Function* F, Module& M) {
  DEBUG(dbgs() << INDENT_5 << "Getting callers for " << F->getName() << "\n");
  if (calleeToCalls.empty()) {
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
          if (fpCallToCallees.find(C) != fpCallToCallees.end()) {
            for (Function* callee : fpCallToCallees[C]) {
              DEBUG(dbgs() << INDENT_3 << "Adding fp-callee " << callee->getName() << "\n");
              callees.push_back(callee);
            }
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
