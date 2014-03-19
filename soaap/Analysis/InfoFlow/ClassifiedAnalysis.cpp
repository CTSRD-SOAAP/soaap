#include "Analysis/InfoFlow/ClassifiedAnalysis.h"
#include "Util/ClassifiedUtils.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "soaap.h"

using namespace soaap;

void ClassifiedAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {

  // initialise with pointers to annotated fields and uses of annotated global variables
  if (Function* F = M.getFunction("llvm.ptr.annotation.p0i8")) {
    for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
      User* user = u->getUser();
      if (isa<IntrinsicInst>(user)) {
        IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(user);
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
  for (Sandbox* S : sandboxes) {
    DEBUG(dbgs() << INDENT_1 << "Sandbox: " << S->getName() << "\n");
    FunctionVector sandboxedFuncs = S->getFunctions();
    int clearances = S->getClearances();
    for (Function* F : sandboxedFuncs) {
      DEBUG(dbgs() << INDENT_1 << "Function: " << F->getName() << ", clearances: " << ClassifiedUtils::stringifyClassNames(clearances) << "\n");
      for (BasicBlock& BB : F->getBasicBlockList()) {
        for (Instruction& I : BB.getInstList()) {
          DEBUG(dbgs() << INDENT_2 << "Instruction: "; I.dump(););
          if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
            Value* v = load->getPointerOperand();
            DEBUG(dbgs() << INDENT_3 << "Value dump: "; v->dump(););
            DEBUG(dbgs() << INDENT_3 << "Value classes: " << state[S][v] << ", " << ClassifiedUtils::stringifyClassNames(state[S][v]) << "\n");
            if (!(state[S][v] == 0 || (state[S][v] & clearances) == state[S][v])) {
              outs() << " *** Sandboxed method \"" << F->getName() << "\" read data value of class: " << ClassifiedUtils::stringifyClassNames(state[S][v]) << " but only has clearances for: " << ClassifiedUtils::stringifyClassNames(clearances) << "\n";
              if (MDNode *N = I.getMetadata("dbg")) {
                DILocation loc(N);
                outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
              }
              outs() << "\n";
            }
          }
          else if (StoreInst* store = dyn_cast<StoreInst>(&I)) {
            Value* v = store->getValueOperand();
            DEBUG(dbgs() << INDENT_3 << "Value dump: "; v->dump(););
            DEBUG(dbgs() << INDENT_3 << "Value classes: " << state[S][v] << ", " << ClassifiedUtils::stringifyClassNames(state[S][v]) << "\n");
            if (!(state[S][v] == 0 || (state[S][v] & clearances) == state[S][v])) {
              outs() << " *** Sandboxed method \"" << F->getName() << "\" read data value of class: " << ClassifiedUtils::stringifyClassNames(state[S][v]) << " but only has clearances for: " << ClassifiedUtils::stringifyClassNames(clearances) << "\n";
              if (MDNode *N = I.getMetadata("dbg")) {
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

bool ClassifiedAnalysis::performMeet(int from, int& to) {
  int oldTo = to;
  to = from | to;
  return to != oldTo;
}

string ClassifiedAnalysis::stringifyFact(int fact) {
  return ClassifiedUtils::stringifyClassNames(fact);
}
