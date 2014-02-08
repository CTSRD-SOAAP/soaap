#include "Analysis/InfoFlow/FPAnnotatedTargetsAnalysis.h"
#include "Analysis/InfoFlow/FPInferredTargetsAnalysis.h"
#include "Common/CmdLineOpts.h"
#include "Passes/Soaap.h"
#include "Util/CallGraphUtils.h"
#include "Util/ClassHierarchyUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>
#include <cxxabi.h>

#include "soaap.h"

using namespace soaap;
using namespace llvm;

map<const CallInst*, FunctionVector> CallGraphUtils::callToCallees;
map<const Function*, CallInstVector> CallGraphUtils::calleeToCalls;
FPAnnotatedTargetsAnalysis CallGraphUtils::fpAnnotatedTargetsAnalysis;
FPInferredTargetsAnalysis CallGraphUtils::fpInferredTargetsAnalysis;
bool CallGraphUtils::caching = false;

void CallGraphUtils::loadDynamicCallGraphEdges(Module& M) {
  /*if (ProfileInfo* PI = LLVMAnalyses::getProfileInfoAnalysis()) {
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
  }*/
  dbgs() << "Dynamic call graph implementation deprecated\n";
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
          if (isIndirectCall(C)) {
            if (MDNode* N = C->getMetadata("dbg")) {
              DILocation loc(N);
              if (!displayedFuncName) {
                // only display function on first function-pointer call
                DEBUG(dbgs() << F->getName() << "\n");
                string funcName = F->getName();
                DEBUG(dbgs() << "got func name\n");
                int status = -4;
                char* demangled = abi::__cxa_demangle(funcName.c_str(), 0, 0, &status);
                DEBUG(dbgs() << "demangled, status=" << status << "\n");
                outs() << INDENT_1 << (status == 0 ? demangled : funcName) << ":\n";
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

void CallGraphUtils::listFPTargets(Module& M) {
  CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
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
    fpInferredTargetsAnalysis.doAnalysis(M, dummyVector);
  }
  fpAnnotatedTargetsAnalysis.doAnalysis(M, dummyVector);

  // for each fp-call, add annotated edges to the call graph
  DEBUG(dbgs() << INDENT_1 << "Finding all fp calls\n");
  if (fpInferredTargetsAnalysis.hasTargets() || fpAnnotatedTargetsAnalysis.hasTargets()) {
    CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
    for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
      if (F->isDeclaration()) continue;
      CallGraphNode* callerNode = CG->getOrInsertFunction(F);
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        if (!isa<IntrinsicInst>(&*I)) {
          if (CallInst* C = dyn_cast<CallInst>(&*I)) {
            if (isIndirectCall(C)) {
              Value* FP = C->getCalledValue();
              DEBUG(dbgs() << INDENT_2 << "Caller: " << C->getParent()->getParent()->getName() << "\n");
              DEBUG(dbgs() << INDENT_2 << "Found fp call: " << *C << "\n");
              DEBUG(dbgs() << INDENT_3 << "FP: " << *FP << "\n");
              DEBUG(dbgs() << INDENT_3 << "Inferred Targets: ";);
              for (Function* T : fpInferredTargetsAnalysis.getTargets(FP)) {
                DEBUG(dbgs() << " " << T->getName());
                CallGraphNode* calleeNode = CG->getOrInsertFunction(T);
                callerNode->addCalledFunction(CallSite(C), calleeNode);
              }
              DEBUG(dbgs() << "\n");
              DEBUG(dbgs() << INDENT_3 << "Annotated Targets: ";);
              for (Function* T : fpAnnotatedTargetsAnalysis.getTargets(FP)) {
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
  }

  // repopulate caches, because they would've been populated already for the FPAnnotatedTargetsAnalysis
  // and now turn on caching so future calls to getCallees and getCallers read from the caches.
  populateCallCalleeCaches(M);
  caching = true;
}

FunctionVector CallGraphUtils::getCallees(const CallInst* C, Module& M) {
  DEBUG(dbgs() << INDENT_5 << "Getting callees for " << *C << "\n");
  if (callToCallees.empty() || !caching) {
    populateCallCalleeCaches(M);
  }
  bool debug = false;
  DEBUG(debug = true);
  if (debug) {
    dbgs() << INDENT_5 << "Callees: ";
    for (Function* F : callToCallees[C]) {
      dbgs() << F->getName() << " ";
    }
    dbgs() << "\n";
  }
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
  map<int,int> calleeCountToFrequencies;
  long numIndCalls = 0;
  long numIndCallees = 0;
  long numVCalls = 0;
  CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  for (Module::iterator F1 = M.begin(), E1 = M.end(); F1 != E1; ++F1) {
    if (F1->isDeclaration()) continue;
    CallGraphNode* F1Node = CG->getOrInsertFunction(F1);
    DEBUG(dbgs() << INDENT_2 << "Processing " << F1->getName() << "\n");
    for (inst_iterator I = inst_begin(F1), E = inst_end(F1); I != E; ++I) {
      if (CallInst* C = dyn_cast<CallInst>(&*I)) {
        FunctionVector callees;
        if (Function* callee = getDirectCallee(C)) {
          if (!callee->isIntrinsic()) {
            DEBUG(dbgs() << INDENT_3 << "Adding callee " << callee->getName() << "\n");
            callees.push_back(callee);
          }
        }
        else if (Value* FP = C->getCalledValue()->stripPointerCasts())  { // dynamic/annotated callees/c++ virtual funcs
          numIndCalls++;
          bool isVCall = C->getMetadata("soaap_defining_vtable_var") != NULL || C->getMetadata("soaap_defining_vtable_name") != NULL;
          /*if (ProfileInfo* PI = LLVMAnalyses::getProfileInfoAnalysis()) {
            for (const Function* callee : PI->getDynamicCallees(C)) {
              DEBUG(dbgs() << INDENT_3 << "Adding dyn-callee " << callee->getName() << "\n");
              callees.push_back((Function*)callee);
            }
          }*/
          for (Function* callee : fpInferredTargetsAnalysis.getTargets(FP)) {
            DEBUG(dbgs() << INDENT_4 << "Adding fp-inferred-callee " << callee->getName() << "\n");
            callees.push_back(callee);
          }
          for (Function* callee : fpAnnotatedTargetsAnalysis.getTargets(FP)) {
            DEBUG(dbgs() << INDENT_4 << "Adding fp-annotated-callee " << callee->getName() << "\n");
            callees.push_back(callee);
          }
          for (Function* callee : ClassHierarchyUtils::getCalleesForVirtualCall(C, M)) {
            DEBUG(dbgs() << INDENT_3 << "Adding virtual-callee " << callee->getName() << "\n");
            callees.push_back(callee);
          }
          numIndCallees += callees.size();
          if (isVCall) {
            calleeCountToFrequencies[callees.size()]++;
            numVCalls++;
            /*if (callees.size() == 529) {
              for (Function* callee : callees) {
                dbgs() << " " << callee->getName() << "\n";
              }
              C->dump();
            }*/
          }
        }
        callToCallees[C] = callees;
        for (Function* callee : callees) {
          calleeToCalls[callee].push_back(C); // we process each C exactly once, so no dups!
          CallGraphNode* calleeNode = CG->getOrInsertFunction(callee);
          F1Node->addCalledFunction(CallSite(C), calleeNode);
        }
      }
    }
  }
  DEBUG(dbgs() << "-----------------------------------------------------------------\n");
  DEBUG(dbgs() << "Outputting callee-count frequencies... (" << numIndCalls << " ind calls, " << numVCalls << " v calls, " <<  numIndCallees << " callees)\n");
  long numIndCalls2 = 0;
  for (map<int,int>::iterator I=calleeCountToFrequencies.begin(), E=calleeCountToFrequencies.end(); I!=E; I++) {
    DEBUG(dbgs() << INDENT_1 << I->first << ": " << I->second << "\n");
    numIndCalls2 += I->second;
  }
  DEBUG(dbgs() << "(Recounted number of indirect calls: " << numIndCalls2 << ")\n");
}

bool CallGraphUtils::isIndirectCall(CallInst* C) {
  Value* V = C->getCalledValue();
  return V != NULL && !isa<Function>(V->stripPointerCasts());
}

Function* CallGraphUtils::getDirectCallee(CallInst* C) {
  Function* calledFunc = C->getCalledFunction();
  if (calledFunc == NULL) {
    calledFunc = dyn_cast<Function>(C->getCalledValue()->stripPointerCasts());
  }
  return calledFunc;
}

