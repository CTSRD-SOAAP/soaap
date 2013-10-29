#include "Analysis/InfoFlow/FPInferredTargetsAnalysis.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/DebugInfo.h"
#include "soaap.h"

#include <sstream>

using namespace soaap;

void FPInferredTargetsAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
  DEBUG(dbgs() << "Running FP inferred targets analysis\n");
  FPTargetsAnalysis::initialise(worklist, M, sandboxes);
  //CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  // find all assignments of functions and propagate them!
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (StoreInst* S = dyn_cast<StoreInst>(&*I)) { // assignments
        //S->dump();
        Value* Rval = S->getValueOperand()->stripPointerCasts();
        if (Function* T = dyn_cast<Function>(Rval)) {
          // we are assigning a function
          FunctionVector targets;
          targets.push_back(T);
          Value* Lvar = S->getPointerOperand()->stripPointerCasts();
          DEBUG(dbgs() << INDENT_1 << "Adding " << *Lvar << " to worklist\n");
          // if lvalue is an annotated struct field, then Lvar will be
          // an intrinsic call to llvm.ptr.annotation.p0i8(%struct, ....), we
          // need to extract %struct in that case
          if (const IntrinsicInst* II = dyn_cast<const IntrinsicInst>(Lvar)) {
            if (II->getIntrinsicID() == Intrinsic::ptr_annotation) { // covers llvm.ptr.annotation.p0i8
              Lvar = II->getArgOperand(0)->stripPointerCasts();
              DEBUG(dbgs() << "Pushing through intrinsic call, now adding " << *Lvar << "\n");
            }
          }
          //dbgs() << *Lvar << " = " << T->getName() << "()\n";
          state[ContextUtils::SINGLE_CONTEXT][Lvar] = targets;
          addToWorklist(Lvar, ContextUtils::SINGLE_CONTEXT, worklist);
        }
      }
      else if (CallInst* C = dyn_cast<CallInst>(&*I)) { // passing functions as params
        if (Function* callee = CallGraphUtils::getDirectCallee(C)) {
          int argIdx = 0;
          for (Function::arg_iterator AI=callee->arg_begin(), AE=callee->arg_end(); AI != AE; AI++, argIdx++) {
            Value* Arg = C->getArgOperand(argIdx)->stripPointerCasts();
            Value* Param = &*AI;
            if (Function* T = dyn_cast<Function>(Arg)) {

              // we are assigning a function
              FunctionVector targets;
              targets.push_back(T);
              //Arg->dump();
              //dbgs() << *Lvar << " = " << T->getName() << "()\n";
              state[ContextUtils::SINGLE_CONTEXT][Param] = targets;
              addToWorklist(Param, ContextUtils::SINGLE_CONTEXT, worklist);
            }
          }
        }
      }
    }
  }
}
