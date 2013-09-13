#ifndef SOAAP_UTILS_CONTEXTUTILS_H
#define SOAAP_UTILS_CONTEXTUTILS_H

#include "llvm/IR/Function.h"
#include "Common/Typedefs.h"
#include "Common/Sandbox.h"

using namespace llvm;

namespace soaap {
  class ContextUtils {
    public:
      static Context* const NO_CONTEXT;
      static Context* const PRIV_CONTEXT;
      static Context* const SINGLE_CONTEXT;

      static bool IsContextInsensitiveAnalysis;
      static Context* calleeContext(Context* C, Function* callee, SandboxVector& sandboxes, Module& M);
      static ContextVector callerContexts(ReturnInst* RI, CallInst* CI, Context* C, SandboxVector& sandboxes, Module& M);
      static ContextVector getContextsForMethod(Function* F, SandboxVector& sandboxes, Module& M);
      static bool isInContext(Instruction* I, Context* C, SandboxVector& sandboxes, Module& M);
      static void setIsContextInsensitiveAnalysis(bool b) { IsContextInsensitiveAnalysis = b; }
      static string stringifyContext(Context* C);
      static ContextVector getAllContexts(SandboxVector& sandboxes);
  };
}

#endif
