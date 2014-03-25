#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "Analysis/InfoFlow/CapabilityAnalysis.h"
#include "Util/LLVMAnalyses.h"
#include "Util/PrettyPrinters.h"
#include "soaap.h"

using namespace soaap;

void CapabilityAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
  for (Sandbox* S : sandboxes) {
    ValueIntMap caps = S->getCapabilities();
    for (pair<const Value*,int> cap : caps) {
      state[S][cap.first] = cap.second;
      addToWorklist(cap.first, S, worklist);
    }
  }
}

bool CapabilityAnalysis::performMeet(int fromVal, int& toVal) {
  int oldToVal = toVal;
  toVal =fromVal & toVal;
  return toVal != oldToVal;
}

void CapabilityAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  validateDescriptorAccesses(M, sandboxes, "read", FD_READ_MASK);
  validateDescriptorAccesses(M, sandboxes, "write", FD_WRITE_MASK);
}

/*
 * Validate that the necessary permissions propagate to the syscall
 */
void CapabilityAnalysis::validateDescriptorAccesses(Module& M, SandboxVector& sandboxes, string syscall, int requiredPerm) {

  SDEBUG("soaap.infoflow.capability", 3, dbgs() << "Validating descriptor accesses for \"" << syscall << "()\"\n");
  if (Function* syscallFn = M.getFunction(syscall)) {
    SDEBUG("soaap.infoflow.capability", 4, dbgs() << syscall << "'s Function* found ( " << syscallFn->getNumUses() << " uses)\n");

    SDEBUG("soaap.infoflow.capability", 4, dbgs() << "Sandboxes: " << SandboxUtils::stringifySandboxVector(sandboxes) << "\n");
    for (Sandbox* S : sandboxes) {
      SDEBUG("soaap.infoflow.capability", 4, dbgs() << "Current sandbox: \"" << S->getName() << "\"\n");
      for (User* U : syscallFn->users()) {
        if (CallInst* Call = dyn_cast<CallInst>(U)) {
          SDEBUG("soaap.infoflow.capability", 4, dbgs() << "Checking call " << *Call << "\n");
          Function* Caller = cast<Function>(Call->getParent()->getParent());
          if (S->containsFunction(Caller)) {
            Value* fd = Call->getArgOperand(0);
            if (!(state[S][fd] & requiredPerm)) {
              outs() << " *** Insufficient privileges for \"" << syscall << "()\" in sandboxed method \"" << Caller->getName() << "\"\n";
              if (MDNode *N = Call->getMetadata("dbg")) {  // Here I is an LLVM instruction
                DILocation Loc(N);                      // DILocation is in DebugInfo.h
                unsigned Line = Loc.getLineNumber();
                StringRef File = Loc.getFilename();
                StringRef Dir = Loc.getDirectory();
                outs() << " +++ Line " << Line << " of file " << File << "\n";
              }
              outs() << "\n";
            }
          }
        }
      }
    }
  }
}

string CapabilityAnalysis::stringifyFact(int fact) {
  return SandboxUtils::stringifySandboxNames(fact);
}
