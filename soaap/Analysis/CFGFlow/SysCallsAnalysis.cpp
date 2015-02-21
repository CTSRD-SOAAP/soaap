#include "Analysis/CFGFlow/SysCallsAnalysis.h"

#include "soaap.h"
#include "Common/CmdLineOpts.h"
#include "Common/Debug.h"
#include "Common/Sandbox.h"
#include "Common/XO.h"
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
  XO::open_list("syscall_warning");
  for (Sandbox* S : sandboxes) {
    SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "sandbox: " << S->getName() << "\n")
    for (CallInst* C : S->getCalls()) {
      SDEBUG("soaap.analysis.cfgflow.syscalls", 4, dbgs() << "call: " << *C << "\n")
      for (Function* Callee : CallGraphUtils::getCallees(C, M)) {
        string funcName = Callee->getName();
        SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "callee: " << funcName << "\n")
        if (freeBSDSysCallProvider.isSysCall(funcName)) {
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
            int idx = freeBSDSysCallProvider.getIdx(funcName);
            SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "syscall idx: " << idx << "\n")
            SDEBUG("soaap.analysis.cfgflow.syscalls", 3, dbgs() << "allowed sys calls vector size and count: " << vector.size() << "," << vector.count() << "\n")
            sysCallAllowed = vector.size() > idx && vector.test(idx);
          }

          // Show warning if system call is not allowed
          if (!sysCallAllowed) {
            XO::open_instance("syscall_warning");
            XO::emit(" *** Sandbox \"{:sandbox/%s}\" performs system call "
                     "\"{:syscall/%s}\" but it is not allowed to,\n"
                     " *** based on the current sandboxing restrictions.\n",
                     S->getName().c_str(),
                     funcName.c_str());
            if (MDNode *N = C->getMetadata("dbg")) {
              DILocation loc(N);
              XO::emit(
                " +++ Line {:line_number/%d} of file {:filename/%s}\n",
                loc.getLineNumber(),
                loc.getFilename().str().c_str());
            }
            // output trace
            if (CmdLineOpts::SysCallTraces) {
              XO::open_list("trace");
              XO::emit(" Possible trace:\n");
              InstTrace callStack = CallGraphUtils::findSandboxedPathToFunction(C->getParent()->getParent(), S, M);
              int currInstIdx = 0;
              bool shownDots = false;
              for (Instruction* I : callStack) {
                if (MDNode *N = I->getMetadata("dbg")) {
                  DILocation Loc(N);
                  Function* EnclosingFunc = cast<Function>(I->getParent()->getParent());
                  unsigned Line = Loc.getLineNumber();
                  StringRef File = Loc.getFilename();
                  unsigned FileOnlyIdx = File.find_last_of("/");
                  StringRef FileOnly = FileOnlyIdx == -1 ? File : File.substr(FileOnlyIdx+1);

                  XO::open_instance("trace");
                  bool printCall = CmdLineOpts::SummariseTraces <= 0 
                                   || currInstIdx < CmdLineOpts::SummariseTraces
                                   || (callStack.size()-(currInstIdx+1))
                                       < CmdLineOpts::SummariseTraces;
                  if (printCall) {
                    XO::emit("      {:function/%s} ",
                             EnclosingFunc->getName().str().c_str());
                    XO::open_container("location");
                    XO::emit("({:file/%s}:{:line/%d})\n",
                             FileOnly.str().c_str(),
                             Line);
                    XO::close_container("location");
                  }
                  else {
                    // output call only in machine-readable reports, and
                    // three lines of "..." otherwise
                    if (!shownDots) {
                      XO::emit("      ...\n");
                      XO::emit("      ...\n");
                      XO::emit("      ...\n");
                      shownDots = true;
                    }
                    XO::emit("{e:function/%s}",
                             EnclosingFunc->getName().str().c_str());
                    XO::open_container("location");
                    XO::emit("{e:file/%s}{e:line/%d}",
                             FileOnly.str().c_str(),
                             Line);
                    XO::close_container("location");
                  }
                  XO::close_instance("trace");
                }
                currInstIdx++;
              }
              XO::emit("\n\n");
              XO::close_list("trace");
            }
            XO::emit("\n");
            XO::close_instance("syscall_warning");
          }
        }
      }
    }
  }
  XO::close_list("syscall_warning");
}

bool SysCallsAnalysis::allowedToPerformNamedSystemCallAtSandboxedPoint(Instruction* I, string sysCall) {
  if (sandboxPlatform) {
    return sandboxPlatform->isSysCallPermitted(sysCall);
  }
  else if (state.find(I) != state.end()) {
    int idx = freeBSDSysCallProvider.getIdx(sysCall);
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
    ss << ((i > 0) ? "," : "") << freeBSDSysCallProvider.getSysCall(idx);
  }
  ss << "]";
  return ss.str();
}
