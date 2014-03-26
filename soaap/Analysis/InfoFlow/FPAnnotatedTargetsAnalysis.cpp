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
  CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  if (Function* F = M.getFunction("llvm.var.annotation")) {
    for (User* U : F->users()) {
      if (IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U)) {
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
          state[ContextUtils::SINGLE_CONTEXT][annotatedVar] = convertFunctionSetToBitVector(callees);
          addToWorklist(annotatedVar, ContextUtils::SINGLE_CONTEXT, worklist);
        }
      }
    }
  }

  if (Function* F = M.getFunction("llvm.ptr.annotation.p0i8")) {
    for (User* U : F->users()) {
      IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U);
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
        state[ContextUtils::SINGLE_CONTEXT][annotateCall] = convertFunctionSetToBitVector(callees);
        addToWorklist(annotateCall, ContextUtils::SINGLE_CONTEXT, worklist);
      }
    }
  }

}
