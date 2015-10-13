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

#include "Analysis/PrivilegedCallAnalysis.h"

#include "soaap.h"
#include "Common/XO.h"
#include "Common/CmdLineOpts.h"
#include "Util/CallGraphUtils.h"
#include "Util/PrettyPrinters.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;

void PrivilegedCallAnalysis::doAnalysis(Module& M, SandboxVector& sandboxes) {
  // first find all methods annotated as being privileged and then check calls within sandboxes
  if (GlobalVariable* lga = M.getNamedGlobal("llvm.global.annotations")) {
    ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
    for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
      ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());

      // get the annotation value first
      GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
      ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
      StringRef annotationStrArrayCString = annotationStrArray->getAsCString();

      GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
      if (isa<Function>(annotatedVal)) {
        Function* annotatedFunc = dyn_cast<Function>(annotatedVal);
        if (annotationStrArrayCString == SOAAP_PRIVILEGED) {
          outs() << "   Found function: " << annotatedFunc->getName() << "\n";
          privAnnotFuncs.push_back(annotatedFunc);
        }
      }
    }
  }          

  // now check calls within sandboxes
  XO::List privilegedCallList("privileged_call");
  for (Function* privilegedFunc : privAnnotFuncs) {
    for (User* U : privilegedFunc->users()) {
      if (CallInst* C = dyn_cast<CallInst>(U)) {
        if (shouldOutputWarningFor(C)) {
          Function* enclosingFunc = C->getParent()->getParent();
          for (Sandbox* S : sandboxes) {
            if (!S->hasCallgate(privilegedFunc) && S->containsFunction(enclosingFunc)) {
              XO::Instance privilegedCallInstance(privilegedCallList);
              XO::emit(" *** Sandbox \"{:sandbox}\" calls privileged function "
                       "\"{:privileged_func/%s}\" that they are not allowed to. "
                       "If intended, annotate this permission using the "
                       "__soaap_callgates annotation.\n\n",
                       S->getName().c_str(),
                       privilegedFunc->getName().str().c_str());
              PrettyPrinters::ppInstruction(C);
              if (CmdLineOpts::isSelected(SoaapAnalysis::PrivCalls, CmdLineOpts::OutputTraces)) {
                CallGraphUtils::emitCallTrace(C->getCalledFunction(), S, M);
              }
            }
          }
        }
      }
    }
  }
  privilegedCallList.close();

  /*
  for (Sandbox* S : sandboxes) {
    FunctionVector callgates = S->getCallgates();
    for (Function* F : S->getFunctions()) {
      for (BasicBlock& BB : F->getBasicBlockList()) {
        for (Instruction& I : BB.getInstList()) {
          if (CallInst* C = dyn_cast<CallInst>(&I)) {
            if (Function* Target = C->getCalledFunction()) {
              if (find(privAnnotFuncs.begin(), privAnnotFuncs.end(), Target) != privAnnotFuncs.end()) {
                // check if this sandbox is allowed to call the privileged function
                DEBUG(dbgs() << "   Found privileged call: "); 
                DEBUG(C->dump());
                if (find(callgates.begin(), callgates.end(), Target) == callgates.end()) {
                  outs() << " *** Sandbox \"" << S->getName() << "\" calls privileged function \"" << Target->getName() << "\" that they are not allowed to. If intended, annotate this permission using the __soaap_callgates annotation.\n";
                  if (MDNode *N = C->getMetadata("dbg")) {  // Here I is an LLVM instruction
                    DILocation Loc(N);                      // DILocation is in DebugInfo.h
                    unsigned Line = Loc.getLineNumber();
                    StringRef File = Loc.getFilename();
                    outs() << " +++ Line " << Line << " of file " << File << "\n";
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  */
}
