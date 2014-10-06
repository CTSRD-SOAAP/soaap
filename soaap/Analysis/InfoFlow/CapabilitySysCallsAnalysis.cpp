#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"

#include <sstream>

#include "Analysis/InfoFlow/CapabilitySysCallsAnalysis.h"
#include "Util/LLVMAnalyses.h"
#include "Util/PrettyPrinters.h"
#include "Util/TypeUtils.h"
#include "soaap.h"

using namespace soaap;

void CapabilitySysCallsAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
  freeBSDSysCallProvider.initSysCalls();
  
  // Add annotations on file descriptor parameters to sandbox entry point
  for (Sandbox* S : sandboxes) {
    ValueFunctionSetMap caps = S->getCapabilities();
    for (pair<const Value*,FunctionSet> cap : caps) {
      function<int (Function*)> func = [&](Function* F) -> int { return freeBSDSysCallProvider.getIdx(F->getName()); };
      state[S][cap.first] = TypeUtils::convertFunctionSetToBitVector(cap.second, func);
      SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << INDENT_2 << "Adding " << *(cap.first) << "\n");
      addToWorklist(cap.first, S, worklist);
    }
  }

  // Find and add file descriptors annotated using __soaap_limit_fd_(key_)?syscalls
  if (Function* F = M.getFunction("llvm.annotation.i32")) {
    for (User* U : F->users()) {
      if (IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U)) {
        Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());
        GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
        ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
        StringRef annotationStrValStr = annotationStrValArray->getAsCString();
        if (annotationStrValStr.startswith(SOAAP_FD_SYSCALLS) || annotationStrValStr.startswith(SOAAP_FD_KEY_SYSCALLS)) {
          FunctionSet sysCalls;
          int subStrStartIdx = (annotationStrValStr.startswith(SOAAP_FD_SYSCALLS) ? strlen(SOAAP_FD_SYSCALLS) : strlen(SOAAP_FD_KEY_SYSCALLS)) + 1; //+1 because of _
          string sysCallListCsv = annotationStrValStr.substr(subStrStartIdx);
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
          BitVector sysCallsVector = convertFunctionSetToBitVector(sysCalls);
          if (annotationStrValStr.startswith(SOAAP_FD_KEY_SYSCALLS)) {
            SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << INDENT_2 << "Annotated var is a fd key\n");                                                                                                      
            if (ConstantInt* CI = dyn_cast<ConstantInt>(annotatedVar)) {
              // currently only support constant/enum key values
              SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "key value is: " << CI->getSExtValue() << "\n");
              SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "allowed system calls: " << stringifyFact(sysCallsVector) << "\n");
              fdKeyToAllowedSysCalls[CI->getSExtValue()] = sysCallsVector;
            }
          }
          else {
            state[ContextUtils::NO_CONTEXT][annotatedVar] = sysCallsVector;
            SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << INDENT_3 << "Initial state: " << stringifyFact(state[ContextUtils::NO_CONTEXT][annotatedVar]) << "\n");

            addToWorklist(annotatedVar, ContextUtils::NO_CONTEXT, worklist);
            ValueSet visited;
            propagateToAggregate(annotatedVar, ContextUtils::NO_CONTEXT, annotatedVar, visited, worklist, sandboxes, M);
            if (ConstantInt* CI = dyn_cast<ConstantInt>(annotatedVar)) {
              SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << INDENT_3 << "Constant integer, val: " << CI->getSExtValue() << ", recording in intFdToAllowedSysCalls");
              intFdToAllowedSysCalls[CI->getSExtValue()] = sysCallsVector;
            }
          }
        }
      }
    }
  }

  // find all calls to fd getters
  if (GlobalVariable* lga = M.getNamedGlobal("llvm.global.annotations")) {
    ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
    for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
      ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());

      // get the annotation value first
      GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
      ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
      StringRef annotationStrArrayCString = annotationStrArray->getAsCString();
      GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
      if (Function* annotatedFunc = dyn_cast<Function>(annotatedVal)) {
        if (annotationStrArrayCString.startswith(SOAAP_FD_GETTER)) {
          SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "Found fd getter function " << annotatedFunc->getName() << "\n");
          // find all callers of annotatedFunc. use CallGraphUtils::getCallers because this
          // is not an intrinsic so we also want to catch calls made via function pointer 
          // and c++ dynamic dispatch
          if (annotatedFunc->getReturnType()->isVoidTy()) {
            errs() << INDENT_1 << "SOAAP ERROR: Function \"" << annotatedFunc->getName() << "\" has been annotated with __soaap_fd_getter but it's return type is void!\n"; 
          }
          else {
            // initialise return values to the worklist and add to the worklist
            int fdKeyIdx = annotatedFunc->getArgumentList().begin()->getName().equals("this") ? 1 : 0;
            for (CallInst* C : CallGraphUtils::getCallers(annotatedFunc, M)) {
              // get fd key value, currently only constants are supported.
              Value* fdKeyArg = C->getArgOperand(fdKeyIdx);
              if (ConstantInt* CI = dyn_cast<ConstantInt>(fdKeyArg)) {
                SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "fd key: " << CI->getSExtValue() << "\n")
                BitVector allowedSysCalls = fdKeyToAllowedSysCalls[CI->getSExtValue()];
                state[ContextUtils::NO_CONTEXT][C] = allowedSysCalls;
                SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << INDENT_3 << "Initial state: " << stringifyFact(state[ContextUtils::NO_CONTEXT][C]) << "\n");
                addToWorklist(C, ContextUtils::NO_CONTEXT, worklist);
              }
            }
          }
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

bool CapabilitySysCallsAnalysis::performUnion(BitVector fromVal, BitVector& toVal) {
  BitVector oldToVal = toVal;
  toVal |= fromVal;
  SDEBUG("soaap.analysis.infoflow.capsyscalls", 4, dbgs() << "fromVal: " << stringifyFact(fromVal) << ", old toVal: " << stringifyFact(oldToVal) << ", new toVal: " << stringifyFact(toVal) << "\n");
  return toVal != oldToVal;
}

// check 
void CapabilitySysCallsAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  //
  // TODO: check that error messages appropriate for both types of annotations
  //
  for (Sandbox* S : sandboxes) {
    SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "sandbox: " << S->getName() << "\n")
    for (CallInst* C : S->getCalls()) {
      SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "call: " << *C << "\n")
      for (Function* Callee : CallGraphUtils::getCallees(C, M)) {
        string funcName = Callee->getName();
        SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "callee: " << funcName << "\n")
        if (freeBSDSysCallProvider.isSysCall(funcName) && freeBSDSysCallProvider.hasFdArg(funcName)) {
          SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "syscall " << funcName << " found and takes fd arg\n")
          // this is a system call
          int fdArgIdx = freeBSDSysCallProvider.getFdArgIdx(funcName);
          int sysCallIdx = freeBSDSysCallProvider.getIdx(funcName);
          Value* fdArg = C->getArgOperand(fdArgIdx);

          SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "fd arg: " << *fdArg << "\n")
          
          // check if there are any restrictions on fdArg and only then determine if this
          // system call is allowed to be called on it (we don't want to be giving errors
          // if the __soaap_limit_fd_syscalls(...) annotation has never been used for fdArg
          //bool restricted = false;
          //if (ConstantInt* CI = dyn_cast<ConstantInt>(fdArg)) {
          //  int fdVal = CI->getSExtValue();
          //  restricted = intFdToAllowedSysCalls.find(fdVal) != intFdToAllowedSysCalls.end();
          //}
          //else {
          //  restricted = state[S].find(fdArg) != state[S].end();
          //}

          //if (restricted) {
            BitVector& vector = isa<ConstantInt>(fdArg) ? intFdToAllowedSysCalls[cast<ConstantInt>(fdArg)->getSExtValue()] : state[S][fdArg];
            SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "syscall idx: " << sysCallIdx << "\n")
            SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "fd arg idx: " << fdArgIdx << "\n")
            if (ConstantInt* CI = dyn_cast<ConstantInt>(fdArg)) {
              SDEBUG("soaap.analysis.infoflow.capsyscalls", 3, dbgs() << "fd arg is a constant, value: " << CI->getSExtValue() << "\n")
            }

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
          //}
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
    if (vector.size() <= idx) {
      vector.resize(idx+1);
    }
    vector.set(idx);
  }
  return vector;
}
