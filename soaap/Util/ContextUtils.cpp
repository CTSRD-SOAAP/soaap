/*
 * Copyright (c) 2013-2015 Khilan Gudka
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "Common/Debug.h"
#include "Util/ContextUtils.h"
#include "Util/SandboxUtils.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

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
  SDEBUG("soaap.util.context", 5, dbgs() << "getContextsForInstruction\n");
  if (contextInsensitive) {
    SDEBUG("soaap.util.context", 5, dbgs() << "context insensitive\n");
    return ContextVector(1, SINGLE_CONTEXT);
  }
  else {
    ContextVector Cs;
    if (SandboxUtils::isPrivilegedInstruction(I, sandboxes, M)) {
      Cs.push_back(PRIV_CONTEXT);
    }
    SDEBUG("soaap.util.context", 5, dbgs() << "looking for sandboxes containing instruction\n");
    SandboxVector containers = SandboxUtils::getSandboxesContainingInstruction(I, sandboxes);
    Cs.insert(Cs.begin(), containers.begin(), containers.end());
    return Cs;
  }
}

bool ContextUtils::isInContext(Instruction* I, Context* C, bool contextInsensitive, SandboxVector& sandboxes, Module& M) {
  ContextVector Cs = getContextsForInstruction(I, contextInsensitive, sandboxes, M);
  SDEBUG("soaap.util.context", 5, dbgs() << "Looking for " << stringifyContext(C) << " amongst " << Cs.size() << " contexts\n");
  SDEBUG("soaap.util.context", 5, dbgs() << "sandboxes.size(): " << sandboxes.size() << "\n");
  return find(Cs.begin(), Cs.end(), C) != Cs.end();
}

string ContextUtils::stringifyContext(Context* C) {
  if (C == PRIV_CONTEXT) {
    return "[<privileged>]";
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
