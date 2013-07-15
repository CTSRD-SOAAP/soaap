#ifndef SOAAP_UTILS_CALLGRAPHUTILS_H
#define SOAAP_UTILS_CALLGRAPHUTILS_H

#include "llvm/IR/Module.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {
  class CallGraphUtils {
    public:
      static void loadDynamicCallGraphEdges(Module& M);
      static FunctionVector getCallees(const CallInst* C, Module& M);
      static CallInstVector getCallers(const Function* F, Module& M);
    
    private:
      static map<const CallInst*, FunctionVector> callToCallees;
      static map<const Function*, CallInstVector> calleeToCalls;
      static void populateCallCalleeCaches(Module& M);
  };
}

#endif
