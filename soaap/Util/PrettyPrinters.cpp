#include "Util/PrettyPrinters.h"
#include "Util/LLVMAnalyses.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;

bool findPathToFuncHelper(CallGraphNode* CurrNode, CallGraphNode* FinalNode, InstTrace& trace, list<CallGraphNode*>& visited, ValueIntMap* shadow, int taint) {
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
        if (shadow) {
          // check that Call has at least one tainted arg
          int idx;
          for (idx = 0; idx < Call->getNumArgOperands(); idx++) {
            if((proceed = ((*shadow)[Call->getArgOperand(idx)->stripPointerCasts()] == taint))) {
              break;
            }
          }
        }
        if (proceed && findPathToFuncHelper(CalleeNode, FinalNode, trace, visited, shadow, taint)) {
          // CurrNode is on a path to FinalNode, so prepend to the trace
          trace.push_back(Call);
          return true;
        }
      }
    }
    return false;
  }
}

InstTrace PrettyPrinters::findPathToFunc(Function* From, Function* To, ValueIntMap* shadow, int taint) {
  CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  CallGraphNode* FromNode = (*CG)[From];
  CallGraphNode* ToNode = (*CG)[To];
  list<CallGraphNode*> visited;
  InstTrace trace;
  findPathToFuncHelper(FromNode, ToNode, trace, visited, shadow, taint);
  return trace;
}

void PrettyPrinters::ppPrivilegedPathToFunction(Function* Target, Module& M) {
  if (Function* MainFn = M.getFunction("main")) {
    // Find privileged path to instruction I, via a function that calls a sandboxed callee
    CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
    CallGraphNode* TargetNode = (*CG)[Target];
    InstTrace trace = findPathToFunc(MainFn, Target, NULL, -1);
    ppTrace(trace);
    outs() << "\n";
  }
}

void PrettyPrinters::ppTaintSource(CallInst* C) {
  outs() << "    Source of untrusted data:\n";
  Function* EnclosingFunc = cast<Function>(C->getParent()->getParent());
  if (MDNode *N = C->getMetadata("dbg")) {
    DILocation Loc(N);
    unsigned Line = Loc.getLineNumber();
    StringRef File = Loc.getFilename();
    unsigned FileOnlyIdx = File.find_last_of("/");
    StringRef FileOnly = FileOnlyIdx == -1 ? File : File.substr(FileOnlyIdx+1);
    outs() << "      " << EnclosingFunc->getName() << "(" << FileOnly << ":" << Line << ")\n";
  }
}

void PrettyPrinters::ppTrace(InstTrace& trace) {
  for (Instruction* I : trace) {
    Function* EnclosingFunc = cast<Function>(I->getParent()->getParent());
    if (MDNode *N = I->getMetadata("dbg")) {
      DILocation Loc(N);
      unsigned Line = Loc.getLineNumber();
      StringRef File = Loc.getFilename();
      unsigned FileOnlyIdx = File.find_last_of("/");
      StringRef FileOnly = FileOnlyIdx == -1 ? File : File.substr(FileOnlyIdx+1);
      outs() << "      " << EnclosingFunc->getName() << "(" << FileOnly << ":" << Line << ")\n";
    }
  }
}
