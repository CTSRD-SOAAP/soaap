#include "Util/ContextUtils.h"
#include "Util/SandboxUtils.h"

using namespace soaap;

bool ContextUtils::IsContextInsensitiveAnalysis = false;

Context* ContextUtils::calleeContext(Context* C, Function* callee, SandboxVector& sandboxes, Module& M) {
  // callee context is the same sandbox, another sandbox or callgate (privileged)
  if (IsContextInsensitiveAnalysis) {
    return InfoFlowAnalysis::SINGLE_CONTEXT;
  }
  else if (SandboxUtils::isSandboxEntryPoint(M, callee)) {
    return SandboxUtils::getSandboxForEntryPoint(callee, sandboxes);
  }
  else if (Sandbox* S = dyn_cast<Sandbox>(C)) {
    // C is a Sandbox 
    if (S->isCallgate(callee)) {
      return InfoFlowAnalysis::PRIV_CONTEXT;
    }
  }
  return C;
}

ContextVector ContextUtils::callerContexts(ReturnInst* RI, CallInst* CI, Context* C, SandboxVector& sandboxes, Module& M) {
  // caller context is the same sandbox or other sandboxes/privileged context (if enclosing function is an entry point)
  if (IsContextInsensitiveAnalysis) {
    return ContextVector(1, InfoFlowAnalysis::SINGLE_CONTEXT);
  }
  else {
    Function* enclosingFunc = RI->getParent()->getParent();
    if (SandboxUtils::isSandboxEntryPoint(M, enclosingFunc)) {
      return getContextsForMethod(CI->getParent()->getParent(), sandboxes, M);
    }
    else {
      return ContextVector(1, C);
    }
  }
}

ContextVector ContextUtils::getContextsForMethod(Function* F, SandboxVector& sandboxes, Module& M) {
  if (IsContextInsensitiveAnalysis) {
    return ContextVector(1, InfoFlowAnalysis::SINGLE_CONTEXT);
  }
  else {
    ContextVector Cs;
    if (SandboxUtils::isPrivilegedMethod(F, M)) {
      Cs.push_back(InfoFlowAnalysis::PRIV_CONTEXT);
    }
    SandboxVector containers = SandboxUtils::getSandboxesContainingMethod(F, sandboxes);
    Cs.insert(Cs.begin(), containers.begin(), containers.end());
    return Cs;
  }
}

bool ContextUtils::isInContext(Instruction* I, Context* C, SandboxVector& sandboxes, Module& M) {
  ContextVector Cs = getContextsForMethod(I->getParent()->getParent(), sandboxes, M);
  return find(Cs.begin(), Cs.end(), C) != Cs.end();
}

string ContextUtils::stringifyContext(Context* C) {
  if (C == InfoFlowAnalysis::PRIV_CONTEXT) {
    return "[<priv>]";
  }
  else if (C == InfoFlowAnalysis::NO_CONTEXT) {
    return "[<none>]";
  }
  else if (C == InfoFlowAnalysis::SINGLE_CONTEXT) {
    return "[<single>]";
  }
  else if (Sandbox* S = dyn_cast<Sandbox>(C)) {
    // sandbox
    return "[" + S->getName() + "]";
  }
  return "null";
}

ContextVector ContextUtils::getAllContexts(SandboxVector& sandboxes) {
  ContextVector Cs;
  Cs.push_back(InfoFlowAnalysis::PRIV_CONTEXT);
  Cs.push_back(InfoFlowAnalysis::NO_CONTEXT);
  Cs.insert(Cs.end(), sandboxes.begin(), sandboxes.end());
  return Cs;
}
