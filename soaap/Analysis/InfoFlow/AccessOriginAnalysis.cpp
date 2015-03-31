#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "Analysis/InfoFlow/AccessOriginAnalysis.h"
#include "Common/Debug.h"
#include "Common/XO.h"
#include "Util/CallGraphUtils.h"
#include "Util/InstUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/PrettyPrinters.h"
#include "Util/PrivInstIterator.h"
#include "Util/SandboxUtils.h"

using namespace soaap;

void AccessOriginAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
  for (Function* F : privilegedMethods) {
    for (PrivInstIterator I = priv_inst_begin(F, sandboxes), E = priv_inst_end(F); I!=E; ++I) {
      if (CallInst* C = dyn_cast<CallInst>(&*I)) {
        for (Function* callee : CallGraphUtils::getCallees(C, ContextUtils::PRIV_CONTEXT, M)) {
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
  XO::List accessOriginList("access_origin_warning");
  for (Function* F : privilegedMethods) {
    for (PrivInstIterator I = priv_inst_begin(F, sandboxes), E = priv_inst_end(F); I!=E; ++I) {
      if (CallInst* C = dyn_cast<CallInst>(&*I)) {
        if (C->getCalledFunction() == NULL) {
          if (shouldOutputWarningFor(C)) {
            if (state[ContextUtils::PRIV_CONTEXT][C->getCalledValue()] == ORIGIN_SANDBOX) {
              XO::Instance accessOriginInstance(accessOriginList);
              XO::emit(" *** Untrusted function pointer call in "
                       "\"{:function/%s}\"\n",
                       F->getName().str().c_str());
              InstUtils::emitInstLocation(C);
              if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                CallGraphUtils::emitCallTrace(F, NULL, M);
              }
              XO::emit("\n");
            }
          }
        }
      }
    }
  }
}


bool AccessOriginAnalysis::performMeet(int from, int& to) {
  return performUnion(from, to);
}

bool AccessOriginAnalysis::performUnion(int from, int& to) {
  int oldTo = to;
  to = from | to;
  return to != oldTo;
}

string AccessOriginAnalysis::stringifyFact(int fact) {
  return SandboxUtils::stringifySandboxNames(fact);
}
