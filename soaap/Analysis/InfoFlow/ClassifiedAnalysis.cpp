#include "Analysis/InfoFlow/ClassifiedAnalysis.h"
#include "Common/XO.h"
#include "Util/ClassifiedUtils.h"
#include "Util/DebugUtils.h"
#include "Util/InstUtils.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "soaap.h"

using namespace soaap;

void ClassifiedAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {

  // initialise with pointers to annotated fields and uses of annotated global variables
  if (Function* F = M.getFunction("llvm.ptr.annotation.p0i8")) {
    for (User* U : F->users()) {
      if (IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U)) {
        Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());

        GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
        ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
        StringRef annotationStrValCString = annotationStrValArray->getAsCString();
        
        if (annotationStrValCString.startswith(CLASSIFY)) {
          StringRef className = annotationStrValCString.substr(strlen(CLASSIFY)+1); //+1 because of _
          ClassifiedUtils::assignBitIdxToClassName(className);
          int bitIdx = ClassifiedUtils::getBitIdxFromClassName(className);
        
          dbgs() << INDENT_1 << "Classification annotation " << annotationStrValCString << " found:\n";
        
          state[ContextUtils::NO_CONTEXT][annotatedVar] |= (1 << bitIdx);
          addToWorklist(annotatedVar, ContextUtils::NO_CONTEXT, worklist);
        }
      }
    }
  }
  
  // annotations on variables are stored in the llvm.global.annotations global
  // array
  if (GlobalVariable* lga = M.getNamedGlobal("llvm.global.annotations")) {
    ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
    for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
      ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());

      // get the annotation value first
      GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
      ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
      StringRef annotationStrArrayCString = annotationStrArray->getAsCString();
      if (annotationStrArrayCString.startswith(CLASSIFY)) {
        GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
        if (isa<GlobalVariable>(annotatedVal)) {
          GlobalVariable* annotatedVar = dyn_cast<GlobalVariable>(annotatedVal);
          if (annotationStrArrayCString.startswith(CLASSIFY)) {
            StringRef className = annotationStrArrayCString.substr(strlen(CLASSIFY)+1);
            ClassifiedUtils::assignBitIdxToClassName(className);
            state[ContextUtils::NO_CONTEXT][annotatedVar] |= (1 << ClassifiedUtils::getBitIdxFromClassName(className));
            addToWorklist(annotatedVar, ContextUtils::NO_CONTEXT, worklist);
          }
        }
      }
    }
  }

}

void ClassifiedAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  // validate that classified data is never accessed inside sandboxed contexts that
  // don't have clearance for its class.
  XO::open_list("classified_warning");
  for (Sandbox* S : sandboxes) {
    SDEBUG("soaap.analysis.infoflow.classified", 3, dbgs() << INDENT_1 << "Sandbox: " << S->getName() << "\n");
    FunctionVector sandboxedFuncs = S->getFunctions();
    int clearances = S->getClearances();
    for (Function* F : sandboxedFuncs) {
      SDEBUG("soaap.analysis.infoflow.classified", 3, dbgs() << INDENT_1 << "Function: " << F->getName() << ", clearances: " << ClassifiedUtils::stringifyClassNames(clearances) << "\n");
      for (BasicBlock& BB : F->getBasicBlockList()) {
        for (Instruction& I : BB.getInstList()) {
          SDEBUG("soaap.analysis.infoflow.classified", 3, dbgs() << INDENT_2 << "Instruction: "; I.dump(););
          Value* V = NULL;
          if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
            V = load->getPointerOperand();
          }
          else if (StoreInst* store = dyn_cast<StoreInst>(&I)) {
            V = store->getValueOperand();
          }

          SDEBUG("soaap.analysis.infoflow.classified", 3, dbgs() << INDENT_3 << "Value dump: "; V->dump(););
          SDEBUG("soaap.analysis.infoflow.classified", 3, dbgs() << INDENT_3 << "Value classes: " << state[S][V] << ", " << ClassifiedUtils::stringifyClassNames(state[S][V]) << "\n");
          if (!(state[S][V] == 0 || (state[S][V] & clearances) == state[S][V])) {
            XO::open_instance("classified_warning");
            XO::emit(" *** Sandboxed method \"{:function/%s}\" "
                     "read data value of class: {d:data_classes/%s} but only "
                     "has clearances for: {d:clearances/%s}\n",
            F->getName().str().c_str(),
            ClassifiedUtils::stringifyClassNames(state[S][V]).c_str(),
            ClassifiedUtils::stringifyClassNames(clearances).c_str());
            StringVector dataClassesVec = ClassifiedUtils::convertNamesToVector(state[S][V]);
            XO::open_list("data_class");
            for (string class_name : dataClassesVec) {
              XO::open_instance("data_class");
              XO::emit("{e:name/%s}", class_name.c_str());
              XO::close_instance("data_class");
            }
            XO::close_list("data_class");
            StringVector clearancesVec = ClassifiedUtils::convertNamesToVector(clearances);
            XO::open_list("clearance");
            for (string clearance : clearancesVec) {
              XO::open_instance("clearance");
              XO::emit("{e:name/%s}", clearance.c_str());
              XO::close_instance("clearance");
            }
            XO::close_list("clearance");
            InstUtils::EmitInstLocation(&I);
            if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
              CallGraphUtils::EmitCallTrace(F, S, M);
            }
            XO::emit("\n");
            XO::close_instance("classified_warning");
          }
        }
      }
    }
  }
  XO::close_list("classified_warning");
}

bool ClassifiedAnalysis::performMeet(int from, int& to) {
  return performUnion(from, to);
}

bool ClassifiedAnalysis::performUnion(int from, int& to) {
  int oldTo = to;
  to = from | to;
  return to != oldTo;
}

string ClassifiedAnalysis::stringifyFact(int fact) {
  return ClassifiedUtils::stringifyClassNames(fact);
}
