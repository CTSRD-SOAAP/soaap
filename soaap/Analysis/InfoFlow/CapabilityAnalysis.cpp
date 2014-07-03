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
  freeBSDSysCallProvider.initSysCalls();
  for (Sandbox* S : sandboxes) {
    ValueFunctionSetMap caps = S->getCapabilities();
    for (pair<const Value*,FunctionSet> cap : caps) {
      function<int (Function*)> func = [&](Function* F) -> int { return freeBSDSysCallProvider.getIdx(F->getName()); };
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
          for (Function* Callee : CallGraphUtils::getCallees(C, M)) {
            string funcName = Callee->getName();
            SDEBUG("soaap.analysis.infoflow.capability", 3, dbgs() << "callee: " << funcName << "\n")
            if (freeBSDSysCallProvider.isSysCall(funcName) && freeBSDSysCallProvider.hasFdArg(funcName)) {
              SDEBUG("soaap.analysis.infoflow.capability", 3, dbgs() << "syscall " << funcName << " found\n")
              // this is a system call
              int fdArgIdx = freeBSDSysCallProvider.getFdArgIdx(funcName);
              int sysCallIdx = freeBSDSysCallProvider.getIdx(funcName);
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

string CapabilityAnalysis::stringifyFact(BitVector vector) {
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
