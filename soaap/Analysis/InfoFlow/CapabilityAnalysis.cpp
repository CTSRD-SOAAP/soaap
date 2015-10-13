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
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "Analysis/InfoFlow/CapabilityAnalysis.h"
#include "Util/LLVMAnalyses.h"
#include "Util/PrettyPrinters.h"
#include "Util/TypeUtils.h"
#include "soaap.h"

#include <sstream>

using namespace soaap;

void CapabilityAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
  for (Sandbox* S : sandboxes) {
    ValueFunctionSetMap caps = S->getCapabilities();
    for (pair<const Value*,FunctionSet> cap : caps) {
      function<int (Function*)> func = [&](Function* F) -> int { return operatingSystem->getIdx(F->getName()); };
      state[S][cap.first] = TypeUtils::convertFunctionSetToBitVector(cap.second, func);
      addToWorklist(cap.first, S, worklist);
    }
  }
}

bool CapabilityAnalysis::performMeet(BitVector fromVal, BitVector& toVal) {
  BitVector oldToVal = toVal;
  toVal &= fromVal;
  return toVal != oldToVal;
}

bool CapabilityAnalysis::performUnion(BitVector fromVal, BitVector& toVal) {
  BitVector oldToVal = toVal;
  toVal |= fromVal;
  return toVal != oldToVal;
}

void CapabilityAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  for (Sandbox* S : sandboxes) {
    SDEBUG("soaap.analysis.infoflow.capability", 3, dbgs() << "sandbox: " << S->getName() << "\n")
    for (Function* F : S->getFunctions()) {
      SDEBUG("soaap.analysis.infoflow.capability", 3, dbgs() << "func: " << F->getName() << "\n")
      for (inst_iterator I=inst_begin(F), E=inst_end(F); I!=E; I++) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          SDEBUG("soaap.analysis.infoflow.capability", 3, dbgs() << "call: " << *C << "\n")
          for (Function* Callee : CallGraphUtils::getCallees(C, S, M)) {
            string funcName = Callee->getName();
            SDEBUG("soaap.analysis.infoflow.capability", 3, dbgs() << "callee: " << funcName << "\n")
            if (operatingSystem->isSysCall(funcName) && operatingSystem->hasFdArg(funcName)) {
              SDEBUG("soaap.analysis.infoflow.capability", 3, dbgs() << "syscall " << funcName << " found\n")
              // this is a system call
              int fdArgIdx = operatingSystem->getFdArgIdx(funcName);
              int sysCallIdx = operatingSystem->getIdx(funcName);
              Value* fdArg = C->getArgOperand(fdArgIdx);
              
              BitVector& vector = state[S][fdArg];
              SDEBUG("soaap.analysis.infoflow.capability", 3, dbgs() << "syscall idx: " << sysCallIdx << "\n")
              SDEBUG("soaap.analysis.infoflow.capability", 3, dbgs() << "fd arg idx: " << fdArgIdx << "\n")
              if (ConstantInt* CI = dyn_cast<ConstantInt>(fdArg)) {
                SDEBUG("soaap.analysis.infoflow.capability", 3, dbgs() << "fd arg is a constant, value: " << CI->getSExtValue() << "\n")
              }

              SDEBUG("soaap.analysis.infoflow.capability", 3, dbgs() << "allowed sys calls vector size and count for fd arg: " << vector.size() << "," << vector.count() << "\n")
              if (vector.size() <= sysCallIdx || !vector.test(sysCallIdx)) {
                outs() << " *** Sandbox \"" << S->getName() << "\" performs system call \"" << funcName << "\"";
                outs() << " but is not allowed to for the given fd arg.\n";
                if (DILocation* loc = dyn_cast_or_null<DILocation>(C->getMetadata("dbg"))) {
                  outs() << " +++ Line " << loc->getLine() << " of file " << loc->getFilename().str() << "\n";
                }
                outs() << "\n";
              }
            }
          }
        }
      }
    }
  }
}

string CapabilityAnalysis::stringifyFact(BitVector vector) {
  stringstream ss;
  ss << "[";
  int idx = 0;
  for (int i=0; i<vector.count(); i++) {
    idx = (i == 0) ? vector.find_first() : vector.find_next(idx);
    ss << ((i > 0) ? "," : "") << operatingSystem->getSysCall(idx);
  }
  ss << "]";
  return ss.str();
}
