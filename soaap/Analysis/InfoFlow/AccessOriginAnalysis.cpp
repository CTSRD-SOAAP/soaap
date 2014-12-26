#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "Analysis/InfoFlow/AccessOriginAnalysis.h"
#include "Common/Debug.h"
#include "Common/XO.h"
#include "Util/CallGraphUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/PrettyPrinters.h"
#include "Util/SandboxUtils.h"

using namespace soaap;

void AccessOriginAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
  for (Function* F : privilegedMethods) {
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I!=E; ++I) {
      if (CallInst* C = dyn_cast<CallInst>(&*I)) {
        for (Function* callee : CallGraphUtils::getCallees(C, M)) {
          if (SandboxUtils::isSandboxEntryPoint(M, callee)) {
            addToWorklist(C, ContextUtils::PRIV_CONTEXT, worklist);
            state[ContextUtils::PRIV_CONTEXT][C] = ORIGIN_SANDBOX;
            untrustedSources.push_back(C);
          }
        }
      }
    }
  }
}

void AccessOriginAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  // check that no untrusted function pointers are called in privileged methods
  XO::open_list("access_origin_warning");
  for (Function* F : privilegedMethods) {
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I!=E; ++I) {
      if (CallInst* C = dyn_cast<CallInst>(&*I)) {
        if (C->getCalledFunction() == NULL) {
          if (state[ContextUtils::PRIV_CONTEXT][C->getCalledValue()] == ORIGIN_SANDBOX) {
            XO::open_instance("access_origin_warning");
            XO::emit(" *** Untrusted function pointer call in "
                     "\"{:function/%s}\"\n",
                     F->getName().str().c_str());
            if (MDNode *N = C->getMetadata("dbg")) {
              DILocation loc(N);
              XO::emit(
                " +++ Line {:line_number/%d} of file {:filename/%s}\n",
                loc.getLineNumber(),
                loc.getFilename().str().c_str());
            }
            XO::emit("\n");
            //ppPrivilegedPathToInstruction(C, M);
            XO::close_instance("access_origin_warning");
          }
        }
      }
    }
  }
  XO::close_list("access_origin_warning");
}


bool AccessOriginAnalysis::performMeet(int from, int& to) {
  return performUnion(from, to);
}

bool AccessOriginAnalysis::performUnion(int from, int& to) {
  int oldTo = to;
  to = from | to;
  return to != oldTo;
}

void AccessOriginAnalysis::ppPrivilegedPathToInstruction(Instruction* I, Module& M) {
  if (Function* MainFn = M.getFunction("main")) {
    // Find privileged path to instruction I, via a function that calls a sandboxed callee
    Function* Target = I->getParent()->getParent();
    CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
    CallGraphNode* TargetNode = (*CG)[Target];

    outs() << "  Possible causes\n";
    for (CallInst* C : untrustedSources) {
      Function* Via = C->getParent()->getParent();
      SDEBUG("soaap.analysis.infoflow.accessorigin", 3, outs() << MainFn->getName() << " -> " << Via->getName() << " -> " << Target->getName() << "\n");
      list<Instruction*> trace1 = PrettyPrinters::findPathToFunc(MainFn, Via, NULL, -1);
      list<Instruction*> trace2 = PrettyPrinters::findPathToFunc(Via, Target, &state[ContextUtils::PRIV_CONTEXT], ORIGIN_SANDBOX);
      // check that we have successfully been able to find a full trace!
      if (!trace1.empty() && !trace2.empty()) {
        PrettyPrinters::ppTaintSource(C);
        // append target instruction I and trace1, to the end of trace2
        trace2.push_front(I);
        trace2.insert(trace2.end(), trace1.begin(), trace1.end());
        PrettyPrinters::ppTrace(trace2);
        outs() << "\n";
      }
    }

    outs() << "Unable to find a trace\n";
  }
}

string AccessOriginAnalysis::stringifyFact(int fact) {
  return SandboxUtils::stringifySandboxNames(fact);
}
