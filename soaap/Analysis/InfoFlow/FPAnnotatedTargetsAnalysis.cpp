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

#include "Analysis/InfoFlow/FPAnnotatedTargetsAnalysis.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include "soaap.h"

#include <sstream>

using namespace soaap;

void FPAnnotatedTargetsAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
  FPTargetsAnalysis::initialise(worklist, M, sandboxes);
  if (Function* F = M.getFunction("llvm.var.annotation")) {
    for (User* U : F->users()) {
      if (IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U)) {
        ContextVector contexts = ContextUtils::getContextsForInstruction(annotateCall, contextInsensitive, sandboxes, M);
        Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());
        GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
        ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
        StringRef annotationStrValStr = annotationStrValArray->getAsCString();
        if (annotationStrValStr.startswith(SOAAP_FP)) {
          FunctionSet callees;
          string funcListCsv = annotationStrValStr.substr(strlen(SOAAP_FP)+1); //+1 because of _
          SDEBUG("soaap.analysis.infoflow.fp.annotate", 3, dbgs() << INDENT_1 << "FP annotation " << annotationStrValStr << " found: " << *annotatedVar << ", funcList: " << funcListCsv << "\n");
          istringstream ss(funcListCsv);
          string func;
          while(getline(ss, func, ',')) {
            // trim leading and trailing spaces
            size_t start = func.find_first_not_of(" ");
            size_t end = func.find_last_not_of(" ");
            func = func.substr(start, end-start+1);
            SDEBUG("soaap.analysis.infoflow.fp.annotate", 3, dbgs() << INDENT_2 << "Function: " << func << "\n");
            if (Function* callee = M.getFunction(func)) {
              SDEBUG("soaap.analysis.infoflow.fp.annotate", 3, dbgs() << INDENT_3 << "Adding " << callee->getName() << "\n");
              callees.insert(callee);
            }
          }
          for (Context* Ctx : contexts) {
            state[Ctx][annotatedVar] = convertFunctionSetToBitVector(callees);
            addToWorklist(annotatedVar, Ctx, worklist);
          }
        }
      }
    }
  }

  if (Function* F = M.getFunction("llvm.ptr.annotation.p0i8")) {
    for (User* U : F->users()) {
      IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U);
      Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());
      ContextVector contexts = ContextUtils::getContextsForInstruction(annotateCall, contextInsensitive, sandboxes, M);

      GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
      ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
      StringRef annotationStrValStr = annotationStrValArray->getAsCString();
      
      if (annotationStrValStr.startswith(SOAAP_FP)) {
        FunctionSet callees;
        string funcListCsv = annotationStrValStr.substr(strlen(SOAAP_FP)+1); //+1 because of _
        SDEBUG("soaap.analysis.infoflow.fp.annotate", 3, dbgs() << INDENT_1 << "FP annotation " << annotationStrValStr << " found: " << *annotatedVar << ", funcList: " << funcListCsv << "\n");
        istringstream ss(funcListCsv);
        string func;
        while(getline(ss, func, ',')) {
          // trim leading and trailing spaces
          size_t start = func.find_first_not_of(" ");
          size_t end = func.find_last_not_of(" ");
          func = func.substr(start, end-start+1);
          SDEBUG("soaap.analysis.infoflow.fp.annotate", 3, dbgs() << INDENT_2 << "Function: " << func << "\n");
          if (Function* callee = M.getFunction(func)) {
            SDEBUG("soaap.analysis.infoflow.fp.annotate", 3, dbgs() << INDENT_3 << "Adding " << callee->getName() << "\n");
            callees.insert(callee);
          }
        }
        for (Context* Ctx : contexts) {
          state[Ctx][annotateCall] = convertFunctionSetToBitVector(callees);
          addToWorklist(annotateCall, Ctx, worklist);
        }
      }
    }
  }

}
