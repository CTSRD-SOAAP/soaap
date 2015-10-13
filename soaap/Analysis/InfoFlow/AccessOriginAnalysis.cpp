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

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "Analysis/InfoFlow/AccessOriginAnalysis.h"
#include "Common/Debug.h"
#include "Common/XO.h"
#include "Util/CallGraphUtils.h"
#include "Util/PrettyPrinters.h"
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
              PrettyPrinters::ppInstruction(C);
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
