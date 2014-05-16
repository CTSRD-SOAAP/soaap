#include "Analysis/CFGFlow/SysCallsAnalysis.h"

#include "soaap.h"
#include "Common/CmdLineOpts.h"
#include "Common/Debug.h"
#include "Common/Sandbox.h"
#include "Util/CallGraphUtils.h"
#include "Util/DebugUtils.h"
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
  freeBSDSysCallProvider.initSysCalls();
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
        int idx = freeBSDSysCallProvider.getIdx(sysCallName);
        SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "idx: " << idx << "\n")
        if (allowedSysCallsBitVector.size() <= idx) {
          allowedSysCallsBitVector.resize(idx+1);
        }
        allowedSysCallsBitVector.set(idx);
        SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "allowed sys calls vector size and count: " << allowedSysCallsBitVector.size() << "," << allowedSysCallsBitVector.count() << "\n")
        state[C] = allowedSysCallsBitVector;
        worklist.enqueue(C->getParent());
      }
    }
  }
}

// check all system calls made within sandboxes
void SysCallsAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  for (Sandbox* S : sandboxes) {
    SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "sandbox: " << S->getName() << "\n")
    for (Function* F : S->getFunctions()) {
      SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "func: " << F->getName() << "\n")
      for (inst_iterator I=inst_begin(F), E=inst_end(F); I!=E; I++) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "call: " << *C << "\n")
          for (Function* Callee : CallGraphUtils::getCallees(C, M)) {
            string funcName = Callee->getName();
            SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "callee: " << funcName << "\n")
            if (freeBSDSysCallProvider.isSysCall(funcName)) {
              SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "syscall " << funcName << " found\n")
              // this is a system call
              int idx = freeBSDSysCallProvider.getIdx(funcName);
              BitVector& vector = state[C];
              SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "syscall idx: " << idx << "\n")
              SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "allowed sys calls vector size and count: " << vector.size() << "," << vector.count() << "\n")
              if (vector.size() <= idx || !vector.test(idx)) {
                outs() << " *** Sandbox \"" << S->getName() << "\" performs system call \"" << funcName << "\"";
                outs() << " but it is not allowed to,\n";
                outs() << " *** based on the current sandboxing restrictions.\n";
                if (MDNode *N = C->getMetadata("dbg")) {
                  DILocation loc(N);
                  outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
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

string SysCallsAnalysis::stringifyFact(BitVector& vector) {
  stringstream ss;
  ss << "[";
  int idx = 0;
  for (int i=0; i<vector.count(); i++) {
    idx = (i == 0) ? vector.find_first() : vector.find_next(idx);
    ss << ((i > 0) ? "," : "") << freeBSDSysCallProvider.getSysCall(idx);
  }
  ss << "]";
  return ss.str();
}
