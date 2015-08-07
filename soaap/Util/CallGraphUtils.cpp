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

map<const CallInst*, map<Context*, FunctionSet> > CallGraphUtils::callToCallees;
map<const Function*, map<Context*, FunctionSet> > CallGraphUtils::funcToCallees;
map<const Function*, map<Context*, set<CallGraphEdge> > > CallGraphUtils::funcToCallEdges;
map<const Function*, map<Context*, CallInstSet> > CallGraphUtils::calleeToCalls;
bool CallGraphUtils::caching = false;
map<Function*, map<Function*,InstTrace> > CallGraphUtils::funcToShortestCallPaths;

DAGNode* CallGraphUtils::bottom = new DAGNode;
map<int,DAGNode*> CallGraphUtils::idToDAGNode;
map<DAGNode*,int> CallGraphUtils::dagNodeToId;

void CallGraphUtils::listFPCalls(Module& M, SandboxVector& sandboxes) {
  unsigned long numFPcalls = 0;
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    bool displayedFuncName = false;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (!isa<IntrinsicInst>(&*I)) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          if (isIndirectCall(C)) {
            if (DILocation* loc = dyn_cast_or_null<DILocation>(C->getMetadata("dbg"))) {
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
              outs() << INDENT_2 << "Call: " << loc->getFilename().str() << ":" << loc->getLine() << "\n";
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

void CallGraphUtils::listFPTargets(Module& M, SandboxVector& sandboxes) {
  unsigned long numFPcalls = 0;
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (!isa<IntrinsicInst>(&*I)) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          if (isIndirectCall(C)) {
            //C->getCalledValue()->stripPointerCasts()->dump();
            if (DILocation* loc = dyn_cast_or_null<DILocation>(C->getMetadata("dbg"))) {
              // only display function on first function-pointer call
              string funcName = F->getName();
              outs() << INDENT_1 << "Function \"" << funcName << "\"\n";
              outs() << INDENT_2 << "Call at " << loc->getFilename().str() << ":" << loc->getLine() << "\n";
              outs() << INDENT_3 << "Targets:\n";
              if (C->getMetadata("soaap_defining_vtable_var") != NULL || C->getMetadata("soaap_defining_vtable_name") != NULL) { // virtual-call
                for (Function* T : ClassHierarchyUtils::getCalleesForVirtualCall(C, M)) {
                  outs() << INDENT_4 << T->getName() << " (inferred virtual)\n";
                }
              }
              ContextVector contexts = ContextUtils::getContextsForInstruction(C, CmdLineOpts::ContextInsens, sandboxes, M);
              for (Context* Ctx : contexts) {
                outs() << INDENT_4 << ContextUtils::stringifyContext(Ctx) << ":\n";
                for (Function* T : getFPAnnotatedTargetsAnalysis().getTargets(C->getCalledValue()->stripPointerCasts(), Ctx)) {
                  outs() << INDENT_5 << T->getName() << " (annotated)\n";
                }
                for (Function* T : getFPInferredTargetsAnalysis().getTargets(C->getCalledValue()->stripPointerCasts(), Ctx)) {
                  outs() << INDENT_5 << T->getName() << " (inferred)\n";
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

void CallGraphUtils::loadAnnotatedInferredCallGraphEdges(Module& M, SandboxVector& sandboxes) {
  // Find annotated/inferred function pointers and add edges from the calls of
  // the fp to targets.  
  if (CmdLineOpts::InferFPTargets) {
    SDEBUG("soaap.util.callgraph", 3, dbgs() << "performing fp target inference\n")
    getFPInferredTargetsAnalysis().doAnalysis(M, sandboxes);
  }
  SDEBUG("soaap.util.callgraph", 3, dbgs() << "finding annotated fp targets\n")
  getFPAnnotatedTargetsAnalysis().doAnalysis(M, sandboxes);

  if (CmdLineOpts::PrintCallGraph) {
    XO::emit("Outputting Callgraph...\n");
    map<Function*,map<Function*,int> > funcToCalleeCallCounts;
    for (pair<const CallInst*, map<Context*, FunctionSet> > p : callToCallees) {
      const CallInst* C = p.first;
      Function* F = (Function*)C->getParent()->getParent();
      for (pair<Context*, FunctionSet> q : p.second) {
        for (Function* G : q.second) {
          funcToCalleeCallCounts[F][G]++;
        }
      }
    }
    XO::List callgraphRecordList("callgraph_record");
    for (pair<Function*,map<Function*,int> > p : funcToCalleeCallCounts) {
      XO::Instance callgraphRecordInstance(callgraphRecordList);
      Function* caller = p.first;
      XO::emit("{:caller/%s}\n", caller->getName().str().c_str());
      XO::List calleeCountList("callee_count");
      for (pair<Function*,int> p2 : p.second) {
        XO::Instance calleeCountInstance(calleeCountList);
        Function* callee = p2.first;
        int callCount = p2.second;
        XO::emit("  -> {:callee/%s}, {:call_count/%d}\n",
                 callee->getName().str().c_str(),
                 callCount);
      }
      XO::emit("\n");
    }
  }

}

FunctionSet CallGraphUtils::getCallees(const CallInst* C, Context* Ctx, Module& M) {
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_5 << "Getting callees for call " << *C << "\n");
  bool debug = false;
  SDEBUG("soaap.util.callgraph", 3, debug = true);
  if (debug) {
    dbgs() << INDENT_5 << "Callees: ";
    for (Function* F : callToCallees[C][Ctx]) {
      dbgs() << F->getName() << " ";
    }
    dbgs() << "\n";
  }
  if (Ctx) {
    return callToCallees[C][Ctx];
  }
  else {
    // merge contexts
    FunctionSet result;
    map<Context*, FunctionSet> ctxToCallees = callToCallees[C];
    for (pair<Context*, FunctionSet> p : ctxToCallees) {
      for (Function* F : p.second) {
        result.insert(F);
      }
    }
    return result;
  }
}

FunctionSet CallGraphUtils::getCallees(const Function* F, Context* Ctx, Module& M) {
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_5 << "Getting callees for function " << F->getName() << "\n");
  bool debug = false;
  SDEBUG("soaap.util.callgraph", 3, debug = true);
  if (debug) {
    dbgs() << INDENT_5 << "Callees: ";
    for (Function* F2 : funcToCallees[F][Ctx]) {
      dbgs() << F->getName() << " ";
    }
    dbgs() << "\n";
  }
  return funcToCallees[F][Ctx];
}

set<CallGraphEdge> CallGraphUtils::getCallGraphEdges(const Function* F, Context* Ctx, Module& M) {
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_5 << "Getting callees for function " << F->getName() << "\n");
  return funcToCallEdges[F][Ctx];
}

CallInstSet CallGraphUtils::getCallers(const Function* F, Context* Ctx, Module& M) {
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_5 << "Getting callers for " << F->getName() << "\n");
  if (Ctx) {
    return calleeToCalls[F][Ctx];
  }
  else {
    CallInstSet result;
    map<Context*, CallInstSet> ctxToCallers = calleeToCalls[F];
    for (pair<Context*, CallInstSet> p : ctxToCallers) {
      for (CallInst* C : p.second) {
        result.insert(C);
      }
    }
    return result;
  }
}

// build basic context-sensitive callgraph using direct callees only
void CallGraphUtils::buildBasicCallGraph(Module& M, SandboxVector& sandboxes) {
  ContextVector contexts = ContextUtils::getAllContexts(sandboxes);

  if (Function* MainFn = M.getFunction("main")) {
    set<Function*> visited;
    FunctionSet initialFuncs;
    initialFuncs.insert(MainFn);
    buildBasicCallGraphHelper(M, sandboxes, initialFuncs, ContextUtils::PRIV_CONTEXT, visited);
  }

  for (Sandbox* S : sandboxes) {
    set<Function*> visited;
    buildBasicCallGraphHelper(M, sandboxes, S->getEntryPoints(), S, visited);
  }

}

void CallGraphUtils::buildBasicCallGraphHelper(Module& M, SandboxVector& sandboxes, FunctionSet initialFuncs, Context* Ctx, set<Function*>& visited) {
  
  vector<Function*> worklist;
  set<Function*> processed;
  
  if (initialFuncs.empty()) {
    // this is the outermost level of a sandboxed region
    worklist.push_back(nullptr);
  }
  else {
    for (Function* F : initialFuncs) {
      worklist.push_back(F);
    }
  }

  while (!worklist.empty()) {
    Function* F = worklist.back();
    worklist.pop_back();

    if (F && F->isDeclaration()) {
      continue;
    }

    if (processed.count(F) > 0) {
      // cycle
      continue;
    }

    // check for change of context. TODO: check callgates
    if (SandboxUtils::isSandboxEntryPoint(M, F) && SandboxUtils::getSandboxForEntryPoint(F, sandboxes) != Ctx) {
      continue;
    }
    
    SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_2 << "Processing " << (F ? F->getName() : "sandboxed region") << "\n");
    
    CallInstVector calls;

    if (!F) {
      // we are at the outermost level of a sandboxed region
      Sandbox* S = dyn_cast<Sandbox>(Ctx);
      for (Instruction* I : S->getRegion()) {
        if (CallInst* C = dyn_cast<CallInst>(I)) {
          calls.push_back(C);
        }
      }
    }
    else {
      processed.insert(F);
      for (inst_iterator I=inst_begin(F), E=inst_end(F); I != E; I++) {
        if (Ctx == ContextUtils::PRIV_CONTEXT && SandboxUtils::isWithinSandboxedRegion(&*I, sandboxes)) {
          continue; // skip
        }
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          calls.push_back(C);
        }
      }
    }

    for (CallInst* C : calls) {
      FunctionSet callees;
      if (Function* callee = getDirectCallee(C)) {
        if (!callee->isIntrinsic()) {
          SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_3 << "Adding direct callee " << callee->getName() << "\n");
          callees.insert(callee);
        }
      }
      else if (Value* FP = C->getCalledValue()->stripPointerCasts())  { // c++ virtual funcs
        bool isVCall = C->getMetadata("soaap_defining_vtable_var") != NULL || C->getMetadata("soaap_defining_vtable_name") != NULL;
        if (isVCall) {
          for (Function* callee : ClassHierarchyUtils::getCalleesForVirtualCall(C, M)) {
            SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_3 << "Adding virtual-callee " << callee->getName() << "\n");
            callees.insert(callee);
          }
        }
      }

      addCallees(C, Ctx, callees, false);
      for (Function* callee : callees) {
        worklist.push_back(callee);
      }
    }
  }

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

void CallGraphUtils::addCallees(CallInst* C, Context* Ctx, FunctionSet& callees, bool reinit) {
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_3 << "New callees to add: " << stringifyFunctionSet(callees) << "\n");
  FunctionSet& currentCallees = (FunctionSet&)callToCallees[C][Ctx];
  Function* EnclosingFunc = C->getParent()->getParent();
  for (Function* callee : callees) {
    if (currentCallees.insert(callee).second) {
      SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_4 << "Adding: " << callee->getName() << "\n");
      calleeToCalls[callee][Ctx].insert(C);
      funcToCallees[EnclosingFunc][Ctx].insert(callee);
      funcToCallEdges[EnclosingFunc][Ctx].insert(CallGraphEdge(C, callee));
    }
  }
  if (reinit) {
    if (Sandbox* S = dyn_cast<Sandbox>(Ctx)) {
      S->reinit();
    }
    else if (Ctx == ContextUtils::PRIV_CONTEXT) {
      Module* M = EnclosingFunc->getParent();
      SandboxUtils::recalculatePrivilegedMethods(*M);
    }
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

bool CallGraphUtils::isReachableFrom(Function* Source, Function* Dest, Sandbox* Ctx, Module& M) {
  set<Function*> visited;
  return isReachableFromHelper(Source, Source, Dest, Ctx, visited, M);
}

bool CallGraphUtils::isReachableFromHelper(Function* Source, Function* Curr, Function* Dest, Sandbox* Ctx, set<Function*>& visited, Module& M) {
  if (Curr == Dest) {
    return true;
  }
  else if (visited.count(Curr) > 0) {
    return false;
  }
  else if (SandboxUtils::isSandboxEntryPoint(M, Curr) && (Ctx != NULL && !Ctx->isEntryPoint(Curr))) {
    return false;
  }
  else {
    visited.insert(Curr);
    for (Function* Succ : getCallees(Curr, Ctx, M)) {
      if (isReachableFromHelper(Source, Succ, Dest, Ctx, visited, M)) {
        return true;
      }
    }
    return false;
  }
}

void CallGraphUtils::calculateShortestCallPathsFromFunc(Function* F, bool privileged, Sandbox* S, Module& M) {
  // we use Dijkstra's algorithm
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "calculating shortest paths from " << F->getName() << " cache");

  // Find privileged path to instruction I, via a function that calls a sandboxed callee
  QueueSet<Function*> worklist;
  unordered_map<Function*,int> distanceFromMain;
  unordered_map<Function*,Function*> pred;
  unordered_map<Function*,CallInst*> call;

  worklist.enqueue(F);
  distanceFromMain[F] = 0;
  pred[F] = NULL;
  call[F] = NULL;

  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "setting all distances from main to INT_MAX\n")
  for (Module::iterator F2 = M.begin(), E = M.end(); F2 != E; ++F2) {
    if (&*F2 != F) {
      distanceFromMain[&*F2] = INT_MAX;
    }
  }

  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "computing dijkstra's algorithm\n")
  while (!worklist.empty()) {
    Function* F2 = worklist.dequeue();
    SDEBUG("soaap.util.callgraph", 4, dbgs() << INDENT_2 << "Current func: " << F2->getName() << "\n")
    // only proceed if:
    // a) in the privileged case, N is not a sandbox entrypoint
    // b) in the non-privileged case, if N is a sandbox entrypoint it must be S's
    // (i.e. in both case we are not entering a different protection domain)
    if (!(privileged && SandboxUtils::isSandboxEntryPoint(M, F2))
        && !(!privileged && SandboxUtils::isSandboxEntryPoint(M, F2) && !S->isEntryPoint(F2))){
      for (CallGraphEdge E : getCallGraphEdges(F2, S ? S : ContextUtils::PRIV_CONTEXT, M)) {
        CallInst* C = E.first;
        Function* SuccFunc = E.second;
        // skip non-main root node
        SDEBUG("soaap.util.callgraph", 4, dbgs() << INDENT_3 << "Succ func: " << SuccFunc->getName() << "\n")
        if (distanceFromMain[SuccFunc] > distanceFromMain[F2]+1) {
          distanceFromMain[SuccFunc] = distanceFromMain[F2]+1;
          pred[SuccFunc] = F2;
          call[SuccFunc] = C;
          worklist.enqueue(SuccFunc);
        }
      }
    }
  }

  // cache shortest paths for each function
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "Caching shortest paths\n")
  for (Module::iterator F2 = M.begin(), E = M.end(); F2 != E; ++F2) {
    if (distanceFromMain[F2] < INT_MAX) { // N is reachable from main
      InstTrace path;
      Function* CurrF = &*F2;
      while (CurrF != F) {
        path.push_back(call[CurrF]);
        CurrF = pred[CurrF];
      }
      funcToShortestCallPaths[F][&*F2] = path;
      SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "Paths from " << F->getName() << "() to " << F2->getName() << ": " << path.size() << "\n")
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
  SDEBUG("soaap.util.callgraph", 3, dbgs() << "finding sandboxed path to function \"" << Target->getName() << "\" (from main)\n");
  InstTrace privStack;
  InstTrace sboxStack;
  if (!S->getEntryPoints().empty()) {
    for (Function* F : S->getEntryPoints()) {
      SDEBUG("soaap.util.callgraph", 3, dbgs() << "finding sandboxed path for function-level sandbox\n");
      if (funcToShortestCallPaths[F].empty()) {
        SDEBUG("soaap.util.callgraph", 3, dbgs() << "populating short call paths (from " << F->getName() << ") cache\n");
        calculateShortestCallPathsFromFunc(F, false, S, M);
      }
      // Find path to Target (in sandbox S) from main() and via S's entrypoint
      privStack = findPrivilegedPathToFunction(F,M);
      sboxStack = funcToShortestCallPaths[F][Target];
    }
  }
  else {
    SDEBUG("soaap.util.callgraph", 3, dbgs() << "finding sandboxed path for sandboxed region\n");
    Function* enclosingFunc = S->getEnclosingFunc();
    privStack = findPrivilegedPathToFunction(enclosingFunc, M);
    CallInstVector calls = S->getTopLevelCalls();
    map<CallInst*,FunctionSet> callToPotentialSrcs;
    // Find those call insts within the region (and immediate callees
    // that can reach Target). In the next phase, we will find the shortest
    // such path.
    for (CallInst* C : calls) {
      SDEBUG("soaap.util.callgraph", 3, dbgs() << "Call: " << *C << "\n");
      FunctionSet callees = getCallees(C, S, M);
      SDEBUG("soaap.util.callgraph", 3, dbgs() << "Callees: " << stringifyFunctionSet(callees) << "\n");
      for (Function* callee : callees) {
        if (funcToShortestCallPaths[callee].empty()) {
          SDEBUG("soaap.util.callgraph", 3, dbgs() << "populating short call paths (from " << callee->getName() << ") cache\n");
          calculateShortestCallPathsFromFunc(callee, false, S, M);
        }
        if (funcToShortestCallPaths[callee].find(Target) != funcToShortestCallPaths[callee].end()) {
          callToPotentialSrcs[C].insert(callee);
        }
      }
    }

    SDEBUG("soaap.util.callgraph", 3, dbgs() << "Finding shortest path from sandboxed region to \"" << Target->getName() << "\""); 
    // Next, find the shortest path from a call inst in the sandboxed region
    // to Target
    tuple<CallInst*,Function*,int> minCallSrc(NULL,NULL,INT_MAX);
    for (CallInst* C : calls) {
      for (Function* src : callToPotentialSrcs[C]) {
        int stackSize = funcToShortestCallPaths[src][Target].size();
        if (stackSize < get<2>(minCallSrc)) {
          minCallSrc = make_tuple(C, src, stackSize);
        }
      }
    }

    if (get<2>(minCallSrc) == INT_MAX) {
      dbgs() << "ERROR: minimum call stack size is still INT_MAX!\n";
    }
    else {
      CallInst* call = get<0>(minCallSrc);
      Function* src = get<1>(minCallSrc);
      int size = get<2>(minCallSrc);
      SDEBUG("soaap.util.callgraph", 3, 
             dbgs() << "Shortest path found from sandboxed region via "
                    << "\"" << src->getName() << "\" to "
                    << "\"" << Target->getName() << "\", size: "
                    << size << "\n");
      // construct sbox stack
      for (Instruction* I : funcToShortestCallPaths[src][Target]) {
        sboxStack.push_back(I);
      }
      sboxStack.push_back(call);
    }
  }

  // join the two together
  for (Instruction* I : privStack) {
    sboxStack.push_back(I);
  }
  SDEBUG("soaap.util.callgraph", 3, dbgs() << "call stack is empty? " << sboxStack.empty() << "\n");
  return sboxStack;
}

int CallGraphUtils::insertIntoTraceDAG(InstTrace& trace) {
  DAGNode* currNode = bottom;
  bool newNode = false;
  for (InstTrace::reverse_iterator I=trace.rbegin(), E=trace.rend(); I!=E; I++) {
    Instruction* inst = *I;
    if (currNode->hasChild(inst)) {
      currNode = currNode->getChild(inst);
    }
    else {
      currNode = currNode->addChild(inst);
      newNode = true;
    }
  }
  int id;
  if (newNode || dagNodeToId.find(currNode) == dagNodeToId.end()) {
    static int nextId = 0;
    id = nextId++;
    //dbgs() << "Setting id to " << id << "\n";
    idToDAGNode[id] = currNode;
    dagNodeToId[currNode] = id;
  }
  else {
    id = dagNodeToId[currNode];
  }
  return id;
}

void CallGraphUtils::emitCallTrace(Function* Target, Sandbox* S, Module& M) {
  XO::emit(" Possible trace ({d:context}):\n", ContextUtils::stringifyContext(S ? S : ContextUtils::PRIV_CONTEXT).c_str());
  InstTrace callStack = S
    ? findSandboxedPathToFunction(Target, S, M)
    : findPrivilegedPathToFunction(Target, M);
  emitCallTrace(callStack);
}

void CallGraphUtils::emitCallTrace(InstTrace callStack) {
  if (callStack.empty()) {
    return;
  }

  int traceId = insertIntoTraceDAG(callStack);
  XO::emit("{e:trace_ref/%s}", ("!trace" + Twine(traceId)).str().c_str());

  int currInstIdx = 0;
  bool shownDots = false;
  for (Instruction* I : callStack) {
    if (DILocation* Loc = dyn_cast_or_null<DILocation>(I->getMetadata("dbg"))) {
      Function* EnclosingFunc = I->getParent()->getParent();
      unsigned Line = Loc->getLine();
      StringRef File = Loc->getFilename();
      unsigned FileOnlyIdx = File.find_last_of("/");
      StringRef FileOnly = FileOnlyIdx == -1 ? File : File.substr(FileOnlyIdx+1);
      string library = DebugUtils::getEnclosingLibrary(I);

      bool printCall = CmdLineOpts::SummariseTraces <= 0
                        || currInstIdx < CmdLineOpts::SummariseTraces
                        || (callStack.size()-(currInstIdx+1))
                            < CmdLineOpts::SummariseTraces;
      if (printCall) {
        XO::emit("      {d:function/%s} ",
                  EnclosingFunc->getName().str().c_str());
        XO::emit("({d:file/%s}:{d:line/%d})",
                  FileOnly.str().c_str(),
                  Line);
        if (!library.empty()) {
          XO::emit(" [{d:library/%s} library]", library.c_str());
        }
        XO::emit("\n");
      }
      else {
        // output call only in machine-readable reports, and
        // three lines of "..." otherwise
        if (!shownDots) {
          XO::emit("      ...\n");
          XO::emit("      ...\n");
          XO::emit("      ...\n");
          shownDots = true;
        }
        /*
        XO::emit("{e:function/%s}",
                  EnclosingFunc->getName().str().c_str());
        XO::Container locationContainer("location");
        XO::emit("{e:file/%s}{e:line/%d}",
                  FileOnly.str().c_str(),
                  Line);
        if (!library.empty()) {
          XO::emit("{e:library/%s}", library.c_str());
        }
        */
      }
    }
    currInstIdx++;
  }
  //XO::emit("\n\n");
}

void CallGraphUtils::emitTraceReferences() {
  for (pair<int,DAGNode*> p : idToDAGNode) {
    int id = p.first;
    DAGNode* currNode = p.second;
    if (currNode == bottom) {
      continue;
    }
    string traceLabel = ("!trace" + Twine(id)).str();
    XO::Container traceContainer(traceLabel.c_str());
    XO::emit("{e:name/%s}", traceLabel.c_str());
    XO::List trace("trace");
    while (currNode != bottom) {
      if (currNode != p.second && dagNodeToId.find(currNode) != dagNodeToId.end()) {
        id = dagNodeToId[currNode];
        XO::Instance traceRefInst(trace);
        XO::emit("{e:trace_ref/%s}", ("!trace" + Twine(id)).str().c_str());
        break;
      }
      else {
        Instruction* I = currNode->getInstruction();
        if (DILocation* Loc = dyn_cast_or_null<DILocation>(I->getMetadata("dbg"))) {
          Function* EnclosingFunc = I->getParent()->getParent();
          unsigned Line = Loc->getLine();
          StringRef File = Loc->getFilename();
          unsigned FileOnlyIdx = File.find_last_of("/");
          StringRef FileOnly = FileOnlyIdx == -1 ? File : File.substr(FileOnlyIdx+1);
          string library = DebugUtils::getEnclosingLibrary(I);
          XO::Instance traceInst(trace);

          XO::emit("{e:function/%s}",
                    EnclosingFunc->getName().str().c_str());
          XO::Container locationContainer("location");
          XO::emit("{e:file/%s}{e:line/%d}",
                    FileOnly.str().c_str(),
                    Line);
          if (!library.empty()) {
            XO::emit("{e:library/%s}", library.c_str());
          }
        }
        currNode = currNode->getParent();
      }
    }
  }
}

FPTargetsAnalysis& CallGraphUtils::getFPInferredTargetsAnalysis() {
  static FPInferredTargetsAnalysis* fpInferredTargetsAnalysis
              = new FPInferredTargetsAnalysis(CmdLineOpts::ContextInsens);
  return *fpInferredTargetsAnalysis;
}

FPTargetsAnalysis& CallGraphUtils::getFPAnnotatedTargetsAnalysis() {
  static FPAnnotatedTargetsAnalysis* fpAnnotatedTargetsAnalysis
              = new FPAnnotatedTargetsAnalysis(CmdLineOpts::ContextInsens);
  return *fpAnnotatedTargetsAnalysis;
}
