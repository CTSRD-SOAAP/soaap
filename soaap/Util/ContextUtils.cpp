#include "Util/ContextUtils.h"
#include "Util/SandboxUtils.h"

using namespace soaap;

Context* const ContextUtils::NO_CONTEXT = new Context();
Context* const ContextUtils::PRIV_CONTEXT = new Context();
Context* const ContextUtils::SINGLE_CONTEXT = new Context();

Context* ContextUtils::calleeContext(Context* C, bool contextInsensitive, Function* callee, SandboxVector& sandboxes, Module& M) {
  // callee context is the same sandbox, another sandbox or callgate (privileged)
  if (contextInsensitive) {
    return SINGLE_CONTEXT;
  }
  else if (SandboxUtils::isSandboxEntryPoint(M, callee)) {
    return SandboxUtils::getSandboxForEntryPoint(callee, sandboxes);
  }
  else if (Sandbox* S = dyn_cast<Sandbox>(C)) {
    // C is a Sandbox 
    if (S->isCallgate(callee)) {
      return PRIV_CONTEXT;
    }
  }
  return C;
}

ContextVector ContextUtils::callerContexts(ReturnInst* RI, CallInst* CI, Context* C, bool contextInsensitive, SandboxVector& sandboxes, Module& M) {
  // caller context is the same sandbox or other sandboxes/privileged context (if enclosing function is an entry point)
  // TODO: what if RI's enclosing function is a callgate?
  if (contextInsensitive) {
    return ContextVector(1, SINGLE_CONTEXT);
  }
  else {
    Function* enclosingFunc = RI->getParent()->getParent();
    if (SandboxUtils::isSandboxEntryPoint(M, enclosingFunc)) {
      return getContextsForMethod(CI->getParent()->getParent(), contextInsensitive, sandboxes, M);
    }
    else {
      return ContextVector(1, C);
    }
  }
}

ContextVector ContextUtils::getContextsForMethod(Function* F, bool contextInsensitive, SandboxVector& sandboxes, Module& M) {
  if (contextInsensitive) {
    return ContextVector(1, SINGLE_CONTEXT);
  }
  else {
    ContextVector Cs;
    if (SandboxUtils::isPrivilegedMethod(F, M)) {
      Cs.push_back(PRIV_CONTEXT);
    }
    SandboxVector containers = SandboxUtils::getSandboxesContainingMethod(F, sandboxes);
    Cs.insert(Cs.begin(), containers.begin(), containers.end());
    return Cs;
  }
}

ContextVector ContextUtils::getContextsForInstruction(Instruction* I, bool contextInsensitive, SandboxVector& sandboxes, Module& M) {
  if (contextInsensitive) {
    return ContextVector(1, SINGLE_CONTEXT);
  }
  else {
    ContextVector Cs;
    if (SandboxUtils::isPrivilegedInstruction(I, sandboxes, M)) {
      Cs.push_back(PRIV_CONTEXT);
    }
    SandboxVector containers = SandboxUtils::getSandboxesContainingInstruction(I, sandboxes);
    Cs.insert(Cs.begin(), containers.begin(), containers.end());
    return Cs;
  }
}

bool ContextUtils::isInContext(Instruction* I, Context* C, bool contextInsensitive, SandboxVector& sandboxes, Module& M) {
  ContextVector Cs = getContextsForInstruction(I, contextInsensitive, sandboxes, M);
  return find(Cs.begin(), Cs.end(), C) != Cs.end();
}

string ContextUtils::stringifyContext(Context* C) {
  if (C == PRIV_CONTEXT) {
    return "[<priv>]";
  }
  else if (C == NO_CONTEXT) {
    return "[<none>]";
  }
  else if (C == SINGLE_CONTEXT) {
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
  Cs.push_back(PRIV_CONTEXT);
  Cs.push_back(NO_CONTEXT);
  Cs.insert(Cs.end(), sandboxes.begin(), sandboxes.end());
  return Cs;
}
