#ifndef SOAAP_UTILS_CONTEXTUTILS_H
#define SOAAP_UTILS_CONTEXTUTILS_H

#include "llvm/IR/Function.h"
#include "Common/Typedefs.h"
#include "Common/Sandbox.h"

#include <stack>

using namespace llvm;
using namespace std;

namespace soaap {
  class ContextUtils {
    public:
      static Context* const NO_CONTEXT;
      static Context* const PRIV_CONTEXT;
      static Context* const SINGLE_CONTEXT;

      static Context* calleeContext(Context* C, Function* callee, SandboxVector& sandboxes, Module& M);
      static ContextVector callerContexts(ReturnInst* RI, CallInst* CI, Context* C, SandboxVector& sandboxes, Module& M);
      static ContextVector getContextsForMethod(Function* F, SandboxVector& sandboxes, Module& M);
      static bool isInContext(Instruction* I, Context* C, SandboxVector& sandboxes, Module& M);
      static void startContextInsensitiveAnalysis() { isContextInsensitiveAnalysisHistory.push(IsContextInsensitiveAnalysis); IsContextInsensitiveAnalysis = true; }
      static void finishContextInsensitiveAnalysis() { IsContextInsensitiveAnalysis = isContextInsensitiveAnalysisHistory.top(); isContextInsensitiveAnalysisHistory.pop(); }
      static bool isContextInsensitiveAnalysis() { return IsContextInsensitiveAnalysis; }
      static string stringifyContext(Context* C);
      static ContextVector getAllContexts(SandboxVector& sandboxes);

    private:
      static bool IsContextInsensitiveAnalysis;
      static stack<bool> isContextInsensitiveAnalysisHistory;
  };
}

#endif
