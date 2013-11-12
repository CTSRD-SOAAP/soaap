#include "llvm/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IntrinsicInst.h"
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

int CapabilityAnalysis::performMeet(int fromVal, int toVal) {
  return fromVal & toVal;
}

void CapabilityAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  validateDescriptorAccesses(M, sandboxes, "read", FD_READ_MASK);
  validateDescriptorAccesses(M, sandboxes, "write", FD_WRITE_MASK);
}

/*
 * Validate that the necessary permissions propagate to the syscall
 */
void CapabilityAnalysis::validateDescriptorAccesses(Module& M, SandboxVector& sandboxes, string syscall, int requiredPerm) {
  if (Function* syscallFn = M.getFunction(syscall)) {
    for (Sandbox* S : sandboxes) {
      FunctionVector sandboxedFuncs = S->getFunctions();
      for (Value::use_iterator I=syscallFn->use_begin(), E=syscallFn->use_end();
           (I != E) && isa<CallInst>(*I); I++) {
        CallInst* Call = cast<CallInst>(*I);
        Function* Caller = cast<Function>(Call->getParent()->getParent());
        if (find(sandboxedFuncs.begin(), sandboxedFuncs.end(), Caller) != sandboxedFuncs.end()) {
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
