#include "Analysis/InfoFlow/FPAnnotatedTargetsAnalysis.h"
#include "Analysis/InfoFlow/FPInferredTargetsAnalysis.h"
#include "Common/CmdLineOpts.h"
#include "Common/XO.h"
#include "Passes/Soaap.h"
#include "Util/CallGraphUtils.h"
#include "Util/ClassHierarchyUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/GraphWriter.h" 
#include "llvm/Support/raw_ostream.h"

#include <sstream>
#include <cxxabi.h>

#include "soaap.h"

using namespace soaap;
using namespace llvm;

map<const CallInst*, FunctionSet> CallGraphUtils::callToCallees;
map<const Function*, CallInstSet> CallGraphUtils::calleeToCalls;
FPAnnotatedTargetsAnalysis CallGraphUtils::fpAnnotatedTargetsAnalysis;
FPInferredTargetsAnalysis CallGraphUtils::fpInferredTargetsAnalysis;
bool CallGraphUtils::caching = false;
map<Function*, map<Function*,InstTrace> > CallGraphUtils::funcToShortestCallPaths;

void CallGraphUtils::loadDynamicCallGraphEdges(Module& M) {
  /*if (ProfileInfo* PI = LLVMAnalyses::getProfileInfoAnalysis()) {
    CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
    for (Module::iterator F1 = M.begin(), E1 = M.end(); F1 != E1; ++F1) {
      if (F1->isDeclaration()) continue;
      for (inst_iterator I = inst_begin(F1), E = inst_end(F1); I != E; ++I) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          for (const Function* F2 : PI->getDynamicCallees(C)) {
            SDEBUG("soaap.util.callgraph", 3, dbgs() << "F2: " << F2->getName() << "\n");
            CallGraphNode* F1Node = CG->getOrInsertFunction(F1);
            CallGraphNode* F2Node = CG->getOrInsertFunction(F2);
            SDEBUG("soaap.util.callgraph", 3, dbgs() << "loadDynamicCallEdges: adding " << F1->getName() << " -> " << F2->getName() << "\n");
            F1Node->addCalledFunction(CallSite(C), F2Node);
          }
        }
      }
    }
  }*/
  //dbgs() << "Dynamic call graph implementation deprecated\n";
}

void CallGraphUtils::listFPCalls(Module& M, SandboxVector& sandboxes) {
  llvm::CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  unsigned long numFPcalls = 0;
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    bool displayedFuncName = false;
    CallGraphNode* callerNode = CG->getOrInsertFunction(F);
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (!isa<IntrinsicInst>(&*I)) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          if (isIndirectCall(C)) {
            if (MDNode* N = C->getMetadata("dbg")) {
              DILocation loc(N);
              if (!displayedFuncName) {
                // only display function on first function-pointer call
                SDEBUG("soaap.util.callgraph", 3, dbgs() << F->getName() << "\n");
                string funcName = F->getName();
                SDEBUG("soaap.util.callgraph", 3, dbgs() << "got func name\n");
                int status = -4;
                char* demangled = abi::__cxa_demangle(funcName.c_str(), 0, 0, &status);
                SDEBUG("soaap.util.callgraph", 3, dbgs() << "demangled, status=" << status << "\n");
                string sandboxed = SandboxUtils::isSandboxedFunction(F, sandboxes) ? " (sandboxed) " : "";
                outs() << INDENT_1 << (status == 0 ? demangled : funcName) << sandboxed << ":\n";
                displayedFuncName = true;
              }
              outs() << INDENT_2 << "Call: " << loc.getFilename().str() << ":" << loc.getLineNumber() << "\n";
              SDEBUG("soaap.util.callgraph", 4, dbgs() << INDENT_3 << "IR instruction: " << *C << "\n");
            }
            numFPcalls++;
          }
        }
      }
    }
  }
  outs() << numFPcalls << " function-pointer calls in total\n";
}

void CallGraphUtils::listFPTargets(Module& M) {
  llvm::CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  unsigned long numFPcalls = 0;
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    CallGraphNode* callerNode = CG->getOrInsertFunction(F);
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (!isa<IntrinsicInst>(&*I)) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          if (isIndirectCall(C)) {
            //C->getCalledValue()->stripPointerCasts()->dump();
            if (MDNode* N = C->getMetadata("dbg")) {
              DILocation loc(N);
              // only display function on first function-pointer call
              string funcName = F->getName();
              outs() << INDENT_1 << "Function \"" << funcName << "\"\n";
              outs() << INDENT_2 << "Call at " << loc.getFilename().str() << ":" << loc.getLineNumber() << "\n";
              outs() << INDENT_3 << "Targets:\n";
              for (Function* T : fpAnnotatedTargetsAnalysis.getTargets(C->getCalledValue()->stripPointerCasts())) {
                outs() << INDENT_4 << T->getName() << " (annotated)\n";
              }
              for (Function* T : fpInferredTargetsAnalysis.getTargets(C->getCalledValue()->stripPointerCasts())) {
                outs() << INDENT_4 << T->getName() << " (inferred)\n";
              }
              if (C->getMetadata("soaap_defining_vtable_var") != NULL || C->getMetadata("soaap_defining_vtable_name") != NULL) { // virtual-call
                for (Function* T : ClassHierarchyUtils::getCalleesForVirtualCall(C, M)) {
                  outs() << INDENT_4 << T->getName() << " (inferred virtual)\n";
                }
              }
              outs() << "\n";
            }
            numFPcalls++;
          }
        }
      }
    }
  }
  outs() << numFPcalls << " function-pointer calls in total\n";
}

void CallGraphUtils::listAllFuncs(Module& M) {
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    outs() << F->getName() << "\n";
  }
}

void CallGraphUtils::loadAnnotatedCallGraphEdges(Module& M) {
  // Find annotated function pointers and add edges from the calls of the fp to targets.
  // Because annotated pointers can be assigned and passed around, we essentially perform
  // an information flow analysis:
  SandboxVector dummyVector;
  //fpAnnotatedTargetsAnalysis.doAnalysis(M, dummyVector);
  if (CmdLineOpts::InferFPTargets) {
    SDEBUG("soaap.util.callgraph", 3, dbgs() << "performing fp target inference\n")
    fpInferredTargetsAnalysis.doAnalysis(M, dummyVector);
  }
  SDEBUG("soaap.util.callgraph", 3, dbgs() << "finding annotated fp targets\n")
  fpAnnotatedTargetsAnalysis.doAnalysis(M, dummyVector);

  // for each fp-call, add annotated edges to the call graph
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "Finding all fp calls\n");
  if (fpInferredTargetsAnalysis.hasTargets() || fpAnnotatedTargetsAnalysis.hasTargets()) {
    llvm::CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
    for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
      if (F->isDeclaration()) continue;
      CallGraphNode* callerNode = CG->getOrInsertFunction(F);
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        if (!isa<IntrinsicInst>(&*I)) {
          if (CallInst* C = dyn_cast<CallInst>(&*I)) {
            if (isIndirectCall(C)) {
              Value* FP = C->getCalledValue();
              SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_2 << "Caller: " << C->getParent()->getParent()->getName() << "\n");
              SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_2 << "Found fp call: " << *C << "\n");
              SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_3 << "FP: " << *FP << "\n");
              SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_3 << "Inferred Targets: ";);
              for (Function* T : fpInferredTargetsAnalysis.getTargets(FP)) {
                SDEBUG("soaap.util.callgraph", 3, dbgs() << " " << T->getName());
                CallGraphNode* calleeNode = CG->getOrInsertFunction(T);
                callerNode->addCalledFunction(CallSite(C), calleeNode);
              }
              SDEBUG("soaap.util.callgraph", 3, dbgs() << "\n");
              SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_3 << "Annotated Targets: ";);
              for (Function* T : fpAnnotatedTargetsAnalysis.getTargets(FP)) {
                SDEBUG("soaap.util.callgraph", 3, dbgs() << " " << T->getName());
                CallGraphNode* calleeNode = CG->getOrInsertFunction(T);
                callerNode->addCalledFunction(CallSite(C), calleeNode);
              }
              SDEBUG("soaap.util.callgraph", 3, dbgs() << "\n");
            }
          }
        }
      }
    }
  }

  // repopulate caches, because they would've been populated already for the FPAnnotatedTargetsAnalysis
  // and now turn on caching so future calls to getCallees and getCallers read from the caches.
  populateCallCalleeCaches(M);
  caching = true;

  if (CmdLineOpts::PrintCallGraph) {
    XO::emit("Outputting Callgraph...\n");
    map<Function*,map<Function*,int> > funcToCalleeCallCounts;
    for (pair<const CallInst*,FunctionSet> p : callToCallees) {
      const CallInst* C = p.first;
      Function* F = (Function*)C->getParent()->getParent();
      for (Function* G : p.second) {
        funcToCalleeCallCounts[F][G]++;
      }
    }
    XO::open_list("callgraph_record");
    for (pair<Function*,map<Function*,int> > p : funcToCalleeCallCounts) {
      XO::open_instance("callgraph_record");
      Function* caller = p.first;
      XO::emit("{:caller/%s}\n", caller->getName().str().c_str());
      XO::open_list("callee_count");
      for (pair<Function*,int> p2 : p.second) {
        XO::open_instance("callee_count");
        Function* callee = p2.first;
        int callCount = p2.second;
        XO::emit("  -> {:callee/%s}, {:call_count/%d}\n",
                 callee->getName().str().c_str(),
                 callCount);
        XO::close_instance("callee_count");
      }
      XO::close_list("callee_count");
      XO::emit("\n");
      XO::close_instance("callgraph_record");
    }
    XO::close_list("callgrap_record");
  }

}

FunctionSet CallGraphUtils::getCallees(const CallInst* C, Module& M) {
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_5 << "Getting callees for " << *C << "\n");
  if (callToCallees.empty() || !caching) {
    populateCallCalleeCaches(M);
  }
  bool debug = false;
  SDEBUG("soaap.util.callgraph", 3, debug = true);
  if (debug) {
    dbgs() << INDENT_5 << "Callees: ";
    for (Function* F : callToCallees[C]) {
      dbgs() << F->getName() << " ";
    }
    dbgs() << "\n";
  }
  return callToCallees[C];
}

CallInstSet CallGraphUtils::getCallers(const Function* F, Module& M) {
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_5 << "Getting callers for " << F->getName() << "\n");
  if (calleeToCalls.empty() || !caching) {
    populateCallCalleeCaches(M);
  }
  return calleeToCalls[F];
}

void CallGraphUtils::populateCallCalleeCaches(Module& M) {
  SDEBUG("soaap.util.callgraph", 3, dbgs() << "-----------------------------------------------------------------\n");
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "Populating call -> callees and callee -> calls cache\n");
  map<int,int> calleeCountToFrequencies;
  long numIndCalls = 0;
  long numIndCallees = 0;
  long numCallees = 0;
  long numDirectCallees = 0;
  long numVCalls = 0;

  // clear caches
  callToCallees.clear();
  calleeToCalls.clear();

  llvm::CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  for (Module::iterator F1 = M.begin(), E1 = M.end(); F1 != E1; ++F1) {
    if (F1->isDeclaration()) continue;
    CallGraphNode* F1Node = CG->getOrInsertFunction(F1);
    SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_2 << "Processing " << F1->getName() << "\n");
    for (inst_iterator I = inst_begin(F1), E = inst_end(F1); I != E; ++I) {
      if (CallInst* C = dyn_cast<CallInst>(&*I)) {
        FunctionSet callees;
        if (Function* callee = getDirectCallee(C)) {
          if (!callee->isIntrinsic()) {
            SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_3 << "Adding callee " << callee->getName() << "\n");
            callees.insert(callee);
            SDEBUG("soaap.util.callgraph", 3, numDirectCallees++);
          }
        }
        else if (Value* FP = C->getCalledValue()->stripPointerCasts())  { // dynamic/annotated callees/c++ virtual funcs
          bool isVCall = C->getMetadata("soaap_defining_vtable_var") != NULL || C->getMetadata("soaap_defining_vtable_name") != NULL;
          /*if (ProfileInfo* PI = LLVMAnalyses::getProfileInfoAnalysis()) {
            for (const Function* callee : PI->getDynamicCallees(C)) {
              SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_3 << "Adding dyn-callee " << callee->getName() << "\n");
              callees.insert((Function*)callee);
            }
          }*/
          if (isVCall) {
            for (Function* callee : ClassHierarchyUtils::getCalleesForVirtualCall(C, M)) {
              SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_3 << "Adding virtual-callee " << callee->getName() << "\n");
              callees.insert(callee);
            }
          }
          if (fpInferredTargetsAnalysis.hasTargets()) {
            for (Function* callee : fpInferredTargetsAnalysis.getTargets(FP)) {
              SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_4 << "Adding fp-inferred-callee " << callee->getName() << "\n");
              callees.insert(callee);
            }
          }
          if (fpAnnotatedTargetsAnalysis.hasTargets()) {
            for (Function* callee : fpAnnotatedTargetsAnalysis.getTargets(FP)) {
              SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_4 << "Adding fp-annotated-callee " << callee->getName() << "\n");
              callees.insert(callee);
            }
          }
          SDEBUG("soaap.util.callgraph", 3, numIndCalls++);
          SDEBUG("soaap.util.callgraph", 3, numIndCallees += callees.size());
          if (isVCall) {
            SDEBUG("soaap.util.callgraph", 3, calleeCountToFrequencies[callees.size()]++);
            SDEBUG("soaap.util.callgraph", 3, numVCalls++);
            /*if (callees.size() == 529) {
              for (Function* callee : callees) {
                dbgs() << " " << callee->getName() << "\n";
              }
              C->dump();
            }*/
          }
        }
        
        // remove declaration-only functions
        /*FunctionSet kill;
        for (Function* F : callees) {
          if (F->isDeclaration()) {
            kill.insert(F);
          }
        }
        for (Function* F : kill) {
          callees.erase(F);
        }*/

        SDEBUG("soaap.util.callgraph", 3, numCallees += callees.size());
        callToCallees[C] = callees;
        for (Function* callee : callees) {
          calleeToCalls[callee].insert(C); // we process each C exactly once, so no dups!
          CallGraphNode* calleeNode = CG->getOrInsertFunction(callee);
          F1Node->addCalledFunction(CallSite(C), calleeNode);
        }
      }
    }
  }
  bool outputStats = false;
  SDEBUG("soaap.util.callgraph", 3, outputStats = true);
  SDEBUG("soaap.util.callgraph", 3, dbgs() << "Direct callees: " << numDirectCallees << ", indirect calls: " << numIndCalls << ", indirect callees: " << numIndCallees << ", numCallees: " << numCallees << "\n");
  if (outputStats) {
    dbgs() << "-----------------------------------------------------------------\n";
    dbgs() << "Outputting callee-count frequencies... (" << numIndCalls << " ind calls, " << numVCalls << " v calls, " <<  numIndCallees << " callees)\n";
    long numIndCalls2 = 0;
    for (map<int,int>::iterator I=calleeCountToFrequencies.begin(), E=calleeCountToFrequencies.end(); I!=E; I++) {
      dbgs() << INDENT_1 << I->first << ": " << I->second << "\n";
      numIndCalls2 += I->second;
    }
    dbgs() << "(Recounted number of indirect calls: " << numIndCalls2 << ")\n";
  }
  caching = true;
}

bool CallGraphUtils::isIndirectCall(CallInst* C) {
  Value* V = C->getCalledValue();
  return V != NULL && !(isa<Function>(V->stripPointerCasts()) || isa<GlobalAlias>(V->stripPointerCasts()));
}

Function* CallGraphUtils::getDirectCallee(CallInst* C) {
  Function* calledFunc = C->getCalledFunction();
  if (calledFunc == NULL) {
    Value* V = C->getCalledValue()->stripPointerCasts();
    calledFunc = dyn_cast<Function>(V);
    if (calledFunc == NULL) {
      if (GlobalAlias* GA = dyn_cast<GlobalAlias>(V)) {
        calledFunc = dyn_cast<Function>(GA->getAliasee());
        //dbgs() << *C << " has direct callee " << calledFunc->getName() << "\n";
      }
    }
  }
  return calledFunc;
}

bool CallGraphUtils::isExternCall(CallInst* C) {
  if (Function* F = getDirectCallee(C)) {
    return F->empty(); // an extern func is one that has no basic blocks
  }
  return false;
}

void CallGraphUtils::addCallees(CallInst* C, FunctionSet& callees) {
  llvm::CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  CallGraphNode* callerNode = CG->getOrInsertFunction(C->getParent()->getParent());
  FunctionSet& currentCallees = (FunctionSet&)callToCallees[C];
  SDEBUG("soaap.util.callgraph", 3, dbgs() << "New callees to add: " << stringifyFunctionSet(callees) << "\n");
  for (Function* callee : callees) {
    if (currentCallees.insert(callee).second) {
      SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "Adding: " << callee->getName() << "\n");
      CallGraphNode* calleeNode = CG->getOrInsertFunction(callee);
      callerNode->addCalledFunction(CallSite(C), calleeNode);
    }
    calleeToCalls[callee].insert(C);
  }
}


string CallGraphUtils::stringifyFunctionSet(FunctionSet& funcs) {
  string funcNamesStr = "[";
  int currIdx = 0;
  bool first = true;
  for (Function* F : funcs) {
    if (!first)
      funcNamesStr += ",";
    funcNamesStr += F->getName();
    first = false;
  }
  funcNamesStr += "]";
  return funcNamesStr;
}

void CallGraphUtils::dumpDOTGraph() {
  DOTGraphTraits<CallGraph*> DT;
  outs() << DT.getGraphName(NULL) << "\n";
  WriteGraph(LLVMAnalyses::getCallGraphAnalysis(), "callgraph");
}

void CallGraphUtils::calculateShortestCallPathsFromFunc(Function* F, bool privileged, Sandbox* S, Module& M) {
  // we use Dijkstra's algorithm
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "calculating shortest paths from " << F->getName() << " cache");
  llvm::CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  CallGraphNode* MainNode = (*CG)[F];

  // Find privileged path to instruction I, via a function that calls a sandboxed callee
  QueueSet<CallGraphNode*> worklist;
  map<CallGraphNode*,int> distanceFromMain;
  map<CallGraphNode*,CallGraphNode*> pred;
  map<CallGraphNode*,CallInst*> call;

  worklist.enqueue(MainNode);
  distanceFromMain[MainNode] = 0;
  pred[MainNode] = NULL;
  call[MainNode] = NULL;

  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "setting all distances from main to INT_MAX\n")
  for (llvm::CallGraph::iterator I=CG->begin(), E=CG->end(); I!= E; I++) {
    CallGraphNode* N = I->second;
    if (N != MainNode) {
      distanceFromMain[N] = INT_MAX;
    }
  }

  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "computing dijkstra's algorithm\n")
  while (!worklist.empty()) {
    CallGraphNode* N = worklist.dequeue();
    SDEBUG("soaap.util.callgraph", 4, dbgs() << INDENT_2 << "Current func: " << N->getFunction()->getName() << "\n")
    // only proceed if:
    // a) in the privileged case, N is not a sandbox entrypoint
    // b) in the non-privileged case, if N is a sandbox entrypoint it must be S's
    // (i.e. in both case we are not entering a different protection domain)
    if (!(privileged && SandboxUtils::isSandboxEntryPoint(M, N->getFunction()))
        && !(!privileged && SandboxUtils::isSandboxEntryPoint(M, N->getFunction()) && N->getFunction() != S->getEntryPoint())){
      for (CallGraphNode::iterator I=N->begin(), E=N->end(); I!=E; I++) {
        CallGraphNode* SuccN = I->second;
        if (Function* SuccFunc = SuccN->getFunction()) {
          SDEBUG("soaap.util.callgraph", 4, dbgs() << INDENT_3 << "Succ func: " << SuccFunc->getName() << "\n")
          if (distanceFromMain[SuccN] > distanceFromMain[N]+1) {
            distanceFromMain[SuccN] = distanceFromMain[N]+1;
            pred[SuccN] = N;
            SDEBUG("soaap.util.callgraph", 4, dbgs() << INDENT_3 << "Call inst: " << *(I->first))
            call[SuccN] = dyn_cast<CallInst>(I->first);
            worklist.enqueue(SuccN);
          }
        }
      }
    }
  }

  // cache shortest paths for each function
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "Caching shortest paths\n")
  for (llvm::CallGraph::iterator I=CG->begin(), E=CG->end(); I!= E; I++) {
    CallGraphNode* N = I->second;
    if (distanceFromMain[N] < INT_MAX) { // N is reachable from main
      InstTrace path;
      CallGraphNode* CurrN = N;
      while (CurrN != MainNode) {
        path.push_back(call[CurrN]);
        CurrN = pred[CurrN];
      }
      funcToShortestCallPaths[F][N->getFunction()] = path;
      SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "Paths from " << F->getName() << "() to " << N->getFunction()->getName() << ": " << path.size() << "\n")
    }
  }

  SDEBUG("soaap.util.callgraph", 4, dbgs() << "completed calculating shortest paths from main cache\n");
}

InstTrace CallGraphUtils::findPrivilegedPathToFunction(Function* Target, Module& M) {
  SDEBUG("soaap.util.callgraph", 3, dbgs() << "finding privileged path to function \"" << Target->getName() << "\" (from main)\n");
  if (Function* MainFn = M.getFunction("main")) {
    if (funcToShortestCallPaths[MainFn].empty()) {
      SDEBUG("soaap.util.callgraph", 3, dbgs() << "populating short call paths (from main) cache\n");
      calculateShortestCallPathsFromFunc(MainFn, true, nullptr, M);
    }
    InstTrace& callStack = funcToShortestCallPaths[MainFn][Target];
    SDEBUG("soaap.util.callgraph", 3, dbgs() << "call stack is empty? " << callStack.empty() << "\n");
    return callStack;
  }
  return InstTrace();
}

InstTrace CallGraphUtils::findSandboxedPathToFunction(Function* Target, Sandbox* S, Module& M) {
  SDEBUG("soaap.util.callgraph", 3, dbgs() << "finding privileged path to function \"" << Target->getName() << "\" (from main)\n");
  Function* F = S->getEntryPoint();
  if (funcToShortestCallPaths[F].empty()) {
    SDEBUG("soaap.util.callgraph", 3, dbgs() << "populating short call paths (from " << F->getName() << ") cache\n");
    calculateShortestCallPathsFromFunc(F, false, S, M);
  }
  // Find path to Target (in sandbox S) from main() and via S's entrypoint
  InstTrace privStack = findPrivilegedPathToFunction(S->getEntryPoint(), M);
  InstTrace sboxStack = funcToShortestCallPaths[F][Target];
  // join the two together
  for (Instruction* I : privStack) {
    sboxStack.push_back(I);
  }
  SDEBUG("soaap.util.callgraph", 3, dbgs() << "call stack is empty? " << sboxStack.empty() << "\n");
  return sboxStack;
}
