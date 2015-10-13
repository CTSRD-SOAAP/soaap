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

#include "Analysis/CFGFlow/SysCallsAnalysis.h"

#include "soaap.h"
#include "Common/CmdLineOpts.h"
#include "Common/Debug.h"
#include "Common/Sandbox.h"
#include "Common/XO.h"
#include "Util/CallGraphUtils.h"
#include "Util/DebugUtils.h"
#include "Util/PrettyPrinters.h"
#include "Util/SandboxUtils.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>

using namespace soaap;

void SysCallsAnalysis::initialise(QueueSet<BasicBlock*>& worklist, Module& M, SandboxVector& sandboxes) {
  for (Sandbox* S : sandboxes) {
    CallInstVector sysCallLimitPoints = S->getSysCallLimitPoints();
    for (CallInst* C : sysCallLimitPoints) {
      SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "syscall limit point: " << *C << "\n")
      FunctionSet allowedSysCalls = S->getAllowedSysCalls(C);  
      SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "allowed sys calls: " << CallGraphUtils::stringifyFunctionSet(allowedSysCalls) << "\n")
      BitVector allowedSysCallsBitVector;

      for (Function* F : allowedSysCalls) {
        string sysCallName = F->getName();
        SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "setting bit for " << sysCallName << "\n")
        int idx = operatingSystem->getIdx(sysCallName);
        if (idx == -1) {
          errs() << "WARNING: \"" << sysCallName << "\" does not appear to be a system call\n";
          continue;
        }
        SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "idx: " << idx << "\n")
        if (allowedSysCallsBitVector.size() <= idx) {
          allowedSysCallsBitVector.resize(idx+1);
        }
        allowedSysCallsBitVector.set(idx);
        SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "allowed sys calls vector size and count: " << allowedSysCallsBitVector.size() << "," << allowedSysCallsBitVector.count() << "\n")
      }

      state[C] = allowedSysCallsBitVector;
      worklist.enqueue(C->getParent());
    }
  }
}

// check all system calls made within sandboxes
void SysCallsAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  XO::List syscallWarningList("syscall_warning");
  for (Sandbox* S : sandboxes) {
    SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "sandbox: " << S->getName() << "\n")
    for (CallInst* C : S->getCalls()) {
      if (shouldOutputWarningFor(C)) {
        SDEBUG("soaap.analysis.cfgflow.syscalls", 4, dbgs() << "call: " << *C << "\n")
        for (Function* Callee : CallGraphUtils::getCallees(C, S, M)) {
          string funcName = Callee->getName();
          SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "callee: " << funcName << "\n")
          if (operatingSystem->isSysCall(funcName)) {
            SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "syscall " << funcName << " found\n")
            bool sysCallAllowed = false;
            if (sandboxPlatform) {
              // sandbox platform dictates if the system call is allowed
              sysCallAllowed = sandboxPlatform->isSysCallPermitted(funcName);
            }
            else if (state.find(C) == state.end()) { // no annotations, so disallow by default
              sysCallAllowed = false;
            }
            else { // there are annotations
              // We distinguish an empty vector from C not appearing in state
              // to avoid blowing up the state map
              BitVector& vector = state[C];
              int idx = operatingSystem->getIdx(funcName);
              SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "syscall idx: " << idx << "\n")
              SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "allowed sys calls vector size and count: " << vector.size() << "," << vector.count() << "\n")
              sysCallAllowed = vector.size() > idx && vector.test(idx);
            }

            // Show warning if system call is not allowed
            if (!sysCallAllowed) {
              XO::Instance syscallWarningInstance(syscallWarningList);
              XO::emit(" *** Sandbox \"{:sandbox/%s}\" performs system call "
                       "\"{:syscall/%s}\" but it is not allowed to,\n"
                       " *** based on the current sandboxing restrictions.\n",
                       S->getName().c_str(),
                       funcName.c_str());
              PrettyPrinters::ppInstruction(C);
              // output trace
              if (CmdLineOpts::isSelected(SoaapAnalysis::SysCalls, CmdLineOpts::OutputTraces)) {
                CallGraphUtils::emitCallTrace(C->getCalledFunction(), S, M);
              }
              XO::emit("\n");
            }
          }
        }
      }
    }
  }
}

bool SysCallsAnalysis::allowedToPerformNamedSystemCallAtSandboxedPoint(Instruction* I, string sysCall) {
  if (sandboxPlatform) {
    return sandboxPlatform->isSysCallPermitted(sysCall);
  }
  else if (state.find(I) != state.end()) {
    int idx = operatingSystem->getIdx(sysCall);
    BitVector& vector = state[I];
    return vector.size() > idx && vector.test(idx);
  }
  return false;
}

string SysCallsAnalysis::stringifyFact(BitVector& vector) {
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
