#include "ADT/QueueSet.h"
#include "Common/CmdLineOpts.h"
#include "Common/Debug.h"
#include "Util/PrettyPrinters.h"
#include "Util/LLVMAnalyses.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;

map<Function*,InstTrace> PrettyPrinters::shortestCallPathsFromMain;

void PrettyPrinters::calculateShortestCallPathsFromMain(Module& M) {
  // we use Dijkstra's algorithm
  if (Function* MainFn = M.getFunction("main")) {
    SDEBUG("soaap.pp", 3, dbgs() << INDENT_1 << "calculating shortest paths from main cache\n")
    CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
    CallGraphNode* MainNode = (*CG)[MainFn];

    // Find privileged path to instruction I, via a function that calls a sandboxed callee
    QueueSet<CallGraphNode*> worklist;
    map<CallGraphNode*,int> distanceFromMain;
    map<CallGraphNode*,CallGraphNode*> pred;
    map<CallGraphNode*,CallInst*> call;

    worklist.enqueue(MainNode);
    distanceFromMain[MainNode] = 0;
    pred[MainNode] = NULL;
    call[MainNode] = NULL;

    SDEBUG("soaap.pp", 3, dbgs() << INDENT_1 << "setting all distances from main to INT_MAX\n")
    for (CallGraph::iterator I=CG->begin(), E=CG->end(); I!= E; I++) {
      CallGraphNode* N = I->second;
      if (N != MainNode) {
        distanceFromMain[N] = INT_MAX;
      }
    }

    SDEBUG("soaap.pp", 3, dbgs() << INDENT_1 << "computing dijkstra's algorithm\n")
    while (!worklist.empty()) {
      CallGraphNode* N = worklist.dequeue();
      SDEBUG("soaap.pp", 4, dbgs() << INDENT_2 << "Current func: " << N->getFunction()->getName() << "\n")
      for (CallGraphNode::iterator I=N->begin(), E=N->end(); I!=E; I++) {
        CallGraphNode* SuccN = I->second;
        if (Function* SuccFunc = SuccN->getFunction()) {
          SDEBUG("soaap.pp", 4, dbgs() << INDENT_3 << "Succ func: " << SuccFunc->getName() << "\n")
          if (distanceFromMain[SuccN] > distanceFromMain[N]+1) {
            distanceFromMain[SuccN] = distanceFromMain[N]+1;
            pred[SuccN] = N;
            SDEBUG("soaap.pp", 4, dbgs() << INDENT_3 << "Call inst: " << *(I->first))
            call[SuccN] = dyn_cast<CallInst>(I->first);
            worklist.enqueue(SuccN);
          }
        }
      }
    }

    // cache shortest paths for each function
    SDEBUG("soaap.pp", 3, dbgs() << INDENT_1 << "Caching shortest paths\n")
    for (CallGraph::iterator I=CG->begin(), E=CG->end(); I!= E; I++) {
      CallGraphNode* N = I->second;
      if (distanceFromMain[N] < INT_MAX) { // N is reachable from main
        InstTrace path;
        CallGraphNode* CurrN = N;
        while (CurrN != MainNode) {
          path.push_back(call[CurrN]);
          CurrN = pred[CurrN];
        }
        shortestCallPathsFromMain[N->getFunction()] = path;
        SDEBUG("soaap.pp", 3, dbgs() << INDENT_1 << "Paths from main() to " << N->getFunction()->getName() << ": " << path.size() << "\n")
      }
    }

    dbgs() << "completed calculating shortest paths from main cache\n";
  }
}

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

  if (shortestCallPathsFromMain.empty()) {
    calculateShortestCallPathsFromMain(M);
  }

  if (Function* MainFn = M.getFunction("main")) {
    // Find privileged path to instruction I, via a function that calls a sandboxed callee
    CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
    InstTrace& callStack = shortestCallPathsFromMain[Target];
    ppTrace(callStack);
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
  bool summariseTrace = CmdLineOpts::SummariseTraces != 0 && (CmdLineOpts::SummariseTraces*2 < trace.size());
  if (summariseTrace) {
    InstTrace::iterator I=trace.begin();
    int i=0;
    for (; i<CmdLineOpts::SummariseTraces; I++, i++) {
      ppInstruction(*I);
    }
    outs() << "      ...\n";
    outs() << "      ...\n";
    outs() << "      ...\n";
    // fast forward to the end
    while (trace.size()-i > CmdLineOpts::SummariseTraces) {
      i++;
      I++;
    }
    for (; i<trace.size(); I++, i++) {
      ppInstruction(*I);
    }
  }
  else {
    for (Instruction* I : trace) {
      ppInstruction(I);
    }
  }
}

void PrettyPrinters::ppInstruction(Instruction* I) {
  if (MDNode *N = I->getMetadata("dbg")) {
    DILocation Loc(N);
    Function* EnclosingFunc = cast<Function>(I->getParent()->getParent());
    unsigned Line = Loc.getLineNumber();
    StringRef File = Loc.getFilename();
    unsigned FileOnlyIdx = File.find_last_of("/");
    StringRef FileOnly = FileOnlyIdx == -1 ? File : File.substr(FileOnlyIdx+1);
    outs() << "      " << EnclosingFunc->getName() << "(" << FileOnly << ":" << Line << ")\n";
  }
}
