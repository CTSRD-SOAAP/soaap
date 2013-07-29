#include "Analysis/PrivilegedCallAnalysis.h"

#include "soaap.h"
#include "llvm/DebugInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;

void PrivilegedCallAnalysis::doAnalysis(Module& M, SandboxVector& sandboxes) {
  // first find all methods annotated as being privileged and then check calls within sandboxes
  if (GlobalVariable* lga = M.getNamedGlobal("llvm.global.annotations")) {
    ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
    for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
      ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());

      // get the annotation value first
      GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
      ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
      StringRef annotationStrArrayCString = annotationStrArray->getAsCString();

      GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
      if (isa<Function>(annotatedVal)) {
        Function* annotatedFunc = dyn_cast<Function>(annotatedVal);
        if (annotationStrArrayCString == SOAAP_PRIVILEGED) {
          outs() << "   Found function: " << annotatedFunc->getName() << "\n";
          privAnnotFuncs.push_back(annotatedFunc);
        }
      }
    }
  }          

  // now check calls within sandboxes
  for (Sandbox* S : sandboxes) {
    FunctionVector callgates = S->getCallgates();
    for (Function* F : S->getFunctions()) {
      for (BasicBlock& BB : F->getBasicBlockList()) {
        for (Instruction& I : BB.getInstList()) {
          if (CallInst* C = dyn_cast<CallInst>(&I)) {
            if (Function* Target = C->getCalledFunction()) {
              if (find(privAnnotFuncs.begin(), privAnnotFuncs.end(), Target) != privAnnotFuncs.end()) {
                // check if this sandbox is allowed to call the privileged function
                DEBUG(dbgs() << "   Found privileged call: "); 
                DEBUG(C->dump());
                if (find(callgates.begin(), callgates.end(), Target) == callgates.end()) {
                  outs() << " *** Sandbox \"" << S->getName() << "\" calls privileged function \"" << Target->getName() << "\" that they are not allowed to. If intended, annotate this permission using the __soaap_callgates annotation.\n";
                  if (MDNode *N = C->getMetadata("dbg")) {  // Here I is an LLVM instruction
                    DILocation Loc(N);                      // DILocation is in DebugInfo.h
                    unsigned Line = Loc.getLineNumber();
                    StringRef File = Loc.getFilename();
                    outs() << " +++ Line " << Line << " of file " << File << "\n";
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
