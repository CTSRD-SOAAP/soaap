#include "Analysis/PrivilegedCallAnalysis.h"

#include "soaap.h"
#include "Common/XO.h"
#include "Common/CmdLineOpts.h"
#include "Util/CallGraphUtils.h"
#include "Util/InstUtils.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
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
  XO::open_list("privileged_call");
  for (Function* privilegedFunc : privAnnotFuncs) {
    for (User* U : privilegedFunc->users()) {
      if (CallInst* C = dyn_cast<CallInst>(U)) {
        Function* enclosingFunc = C->getParent()->getParent();
        for (Sandbox* S : sandboxes) {
          if (!S->hasCallgate(privilegedFunc) && S->containsFunction(enclosingFunc)) {
            XO::open_instance("privileged_call");
            XO::emit(" *** Sandbox \"{:sandbox}\" calls privileged function "
                     "\"{:privileged_func/%s}\" that they are not allowed to. "
                     "If intended, annotate this permission using the "
                     "__soaap_callgates annotation.\n",
                     S->getName().c_str(),
                     privilegedFunc->getName().str().c_str());
            InstUtils::emitInstLocation(C);
            if (CmdLineOpts::isSelected(SoaapAnalysis::PrivCalls, CmdLineOpts::OutputTraces)) {
              CallGraphUtils::emitCallTrace(C->getCalledFunction(), S, M);
            }
            XO::close_instance("privileged_call");
          }
        }
      }
    }
  }
  XO::close_list("privileged_call");

  /*
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
  */
}
