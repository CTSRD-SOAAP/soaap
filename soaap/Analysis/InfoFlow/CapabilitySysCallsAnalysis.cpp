#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"

#include <sstream>

#include "Analysis/InfoFlow/CapabilitySysCallsAnalysis.h"
#include "Util/LLVMAnalyses.h"
#include "Util/PrettyPrinters.h"
#include "soaap.h"

using namespace soaap;

void CapabilitySysCallsAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
  freeBSDSysCallProvider.initSysCalls();
  if (Function* F = M.getFunction("llvm.annotation.i32")) {
    for (User* U : F->users()) {
      if (IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U)) {
        Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());
        GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
        ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
        StringRef annotationStrValStr = annotationStrValArray->getAsCString();
        if (annotationStrValStr.startswith(SOAAP_FD_SYSCALLS)) {
          FunctionSet sysCalls;
          string sysCallListCsv = annotationStrValStr.substr(strlen(SOAAP_FD_SYSCALLS)+1); //+1 because of _
          SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << INDENT_1 << " " << annotationStrValStr << " found: " << *annotatedVar << ", sysCallList: " << sysCallListCsv << "\n");
          istringstream ss(sysCallListCsv);
          string sysCallName;
          while(getline(ss, sysCallName, ',')) {
            // trim leading and trailing spaces
            size_t start = sysCallName.find_first_not_of(" ");
            size_t end = sysCallName.find_last_not_of(" ");
            sysCallName = sysCallName.substr(start, end-start+1);
            SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << INDENT_2 << "Syscall: " << sysCallName << "\n");
            if (Function* sysCallFn = M.getFunction(sysCallName)) {
              SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << INDENT_3 << "Adding " << sysCallFn->getName() << "\n");
              sysCalls.insert(sysCallFn);
            }
          }
          state[ContextUtils::NO_CONTEXT][annotatedVar] = convertFunctionSetToBitVector(sysCalls);
          SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << INDENT_3 << "Initial state: " << stringifyFact(state[ContextUtils::NO_CONTEXT][annotatedVar]) << "\n");

          addToWorklist(annotatedVar, ContextUtils::NO_CONTEXT, worklist);
          ValueSet visited;
          propagateToAggregate(annotatedVar, ContextUtils::NO_CONTEXT, annotatedVar, visited, worklist, sandboxes, M);
        }
      }
    }
  }
}

bool CapabilitySysCallsAnalysis::performMeet(BitVector fromVal, BitVector& toVal) {
  BitVector oldToVal = toVal;
  toVal &= fromVal;
  SDEBUG("soaap.analysis.infoflow.capsyscalls", 4, dbgs() << "fromVal: " << stringifyFact(fromVal) << ", old toVal: " << stringifyFact(oldToVal) << ", new toVal: " << stringifyFact(toVal) << "\n");
  return toVal != oldToVal;
}

// check 
void CapabilitySysCallsAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  for (Sandbox* S : sandboxes) {
    SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "sandbox: " << S->getName() << "\n")
    for (Function* F : S->getFunctions()) {
      SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "func: " << F->getName() << "\n")
      for (inst_iterator I=inst_begin(F), E=inst_end(F); I!=E; I++) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "call: " << *C << "\n")
          for (Function* Callee : CallGraphUtils::getCallees(C, M)) {
            string funcName = Callee->getName();
            SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "callee: " << funcName << "\n")
            if (freeBSDSysCallProvider.isSysCall(funcName) && freeBSDSysCallProvider.hasFdArg(funcName)) {
              SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "syscall " << funcName << " found\n")
              // this is a system call
              int fdArgIdx = freeBSDSysCallProvider.getFdArgIdx(funcName);
              int sysCallIdx = freeBSDSysCallProvider.getIdx(funcName);
              Value* fdArg = C->getArgOperand(fdArgIdx);
              BitVector& vector = state[S][fdArg];
              SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "syscall idx: " << sysCallIdx << "\n")
              SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "fd arg idx: " << fdArgIdx << "\n")
              SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "allowed sys calls vector size and count for fd arg: " << vector.size() << "," << vector.count() << "\n")
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

string CapabilitySysCallsAnalysis::stringifyFact(BitVector vector) {
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

BitVector CapabilitySysCallsAnalysis::convertFunctionSetToBitVector(FunctionSet sysCalls) {
  BitVector vector;
  for (Function* F : sysCalls) {
    int idx = freeBSDSysCallProvider.getIdx(F->getName());
    vector.resize(idx+1);
    vector.set(idx);
  }
  return vector;
}
