#ifndef SOAAP_UTILS_CALLGRAPHUTILS_H
#define SOAAP_UTILS_CALLGRAPHUTILS_H

#include "llvm/IR/Module.h"
#include "llvm/Support/GraphWriter.h"
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
      static string stringifyFunctionSet(FunctionSet& funcs);
      static void dumpDOTGraph();
      static InstTrace findPrivilegedPathToFunction(Function* Target, Module& M);
      static InstTrace findSandboxedPathToFunction(Function* Target, Sandbox* S, Module& M);
      static bool isReachableFrom(Function* Source, Function* Dest, Sandbox* Ctx, Module& M);
      /**
       * emits a call trace to @p Target for the given sandbox @p S.
       * If @p S is null then a privileged call graph will be emitted instead.
       */
      static void EmitCallTrace(Function* Target, Sandbox* S, Module& M);
    private:
      static map<const CallInst*, FunctionSet> callToCallees;
      static map<const Function*, CallInstSet> calleeToCalls;
      static map<Function*, map<Function*,InstTrace> > funcToShortestCallPaths;
      static FPAnnotatedTargetsAnalysis fpAnnotatedTargetsAnalysis;
      static FPInferredTargetsAnalysis fpInferredTargetsAnalysis;
      static bool caching;
      static void populateCallCalleeCaches(Module& M);
      static void calculateShortestCallPathsFromFunc(Function* F, bool privileged, Sandbox* S, Module& M);
      static bool isReachableFromHelper(Function* Source, Function* Curr, Function* Dest, Sandbox* Ctx, set<Function*>& visited, Module& M);
  };
}
namespace llvm {
  template<>
  struct DOTGraphTraits<CallGraph*> : public DefaultDOTGraphTraits {
    DOTGraphTraits (bool isSimple=false) : DefaultDOTGraphTraits(isSimple) {}
  };
}
#endif
