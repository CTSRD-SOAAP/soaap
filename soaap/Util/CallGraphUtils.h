#ifndef SOAAP_UTILS_CALLGRAPHUTILS_H
#define SOAAP_UTILS_CALLGRAPHUTILS_H

#include "llvm/IR/Module.h"
#include "Common/Sandbox.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {
  class FPAnnotatedTargetsAnalysis;
  class FPInferredTargetsAnalysis;
  class CallGraphUtils {
    public:
      static void loadDynamicCallGraphEdges(Module& M);
      static void loadAnnotatedCallGraphEdges(Module& M);
      static void listFPCalls(Module& M, SandboxVector& sandboxes);
      static void listFPTargets(Module& M);
      static void listAllFuncs(Module& M);
      static bool isIndirectCall(CallInst* C);
      static Function* getDirectCallee(CallInst* C);
      static FunctionSet getCallees(const CallInst* C, Module& M);
      static CallInstSet getCallers(const Function* F, Module& M);
      static bool isExternCall(CallInst* C);
      static void addCallees(CallInst* C, FunctionSet& callees);
    
    private:
      static map<const CallInst*, FunctionSet> callToCallees;
      static map<const Function*, CallInstSet> calleeToCalls;
      static FPAnnotatedTargetsAnalysis fpAnnotatedTargetsAnalysis;
      static FPInferredTargetsAnalysis fpInferredTargetsAnalysis;
      static bool caching;
      static void populateCallCalleeCaches(Module& M);
  };
}

#endif
