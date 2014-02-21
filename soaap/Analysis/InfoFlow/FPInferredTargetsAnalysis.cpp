#include "Analysis/InfoFlow/FPInferredTargetsAnalysis.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/DebugInfo.h"
#include "soaap.h"

#include <sstream>

using namespace soaap;

void FPInferredTargetsAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {
  //llvm::DebugFlag = true;
  DEBUG(dbgs() << "Running FP inferred targets analysis\n");
  FPTargetsAnalysis::initialise(worklist, M, sandboxes);
  //CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  // find all assignments of functions and propagate them!
  long numFuncs = 0;
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    numFuncs++;
  }
  DEBUG(dbgs() << "Num of funcs: " << numFuncs << "\n");

  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    DEBUG(dbgs() << F->getName() << "\n");
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (StoreInst* S = dyn_cast<StoreInst>(&*I)) { // assignments
        //DEBUG(dbgs() << F->getName() << ": " << *S);
        Value* Rval = S->getValueOperand()->stripInBoundsConstantOffsets();
        if (Function* T = dyn_cast<Function>(Rval)) {
          if (!T->isDeclaration()) {
            // we are assigning a function
            Value* Lvar = S->getPointerOperand()->stripInBoundsOffsets();
            //DEBUG(dbgs() << INDENT_1 << "Adding " << *Lvar << " to worklist\n");
            // if lvalue is an annotated struct field, then Lvar will be
            // an intrinsic call to llvm.ptr.annotation.p0i8(%struct, ....), we
            // need to extract %struct in that case
            if (const IntrinsicInst* II = dyn_cast<const IntrinsicInst>(Lvar)) {
              if (II->getIntrinsicID() == Intrinsic::ptr_annotation) { // covers llvm.ptr.annotation.p0i8
                Lvar = II->getArgOperand(0)->stripInBoundsOffsets();
                DEBUG(dbgs() << "Pushing through intrinsic call, now adding " << *Lvar << "\n");
              }
            }
            // propagate it all the way back to either an alloca or global var
            // because if we are assigning to a field of a struct, the assignment
            // may get lost at lower optimisation levels because the alloca will
            // be reloaded from for subsequent dereferences.
            DEBUG(dbgs() << "Rewinding back to alloca\n");
            while (!(isa<AllocaInst>(Lvar) || isa<GlobalVariable>(Lvar))) {
              if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(Lvar)) {
                Lvar = gep->getPointerOperand();
              }
              else if (LoadInst* load = dyn_cast<LoadInst>(Lvar)) {
                Lvar = load->getPointerOperand();
              }
              else {
                dbgs() << "WARNING: unexpected instruction: " << *Lvar << "\n";
              }
              Lvar = Lvar->stripInBoundsOffsets();
            }
            DEBUG(dbgs() << *Lvar << " = " << T->getName() << "()\n");
            state[ContextUtils::SINGLE_CONTEXT][Lvar].insert(T);
            addToWorklist(Lvar, ContextUtils::SINGLE_CONTEXT, worklist);

            // if Lvar is a struct parameter, then it probably outlives this function and
            // so we should propagate the targets of function pointers it contains to the 
            // calling context (i.e. to the corresponding caller's arg value)
            //TODO: how do we know A is specifically a struct parameter?
            if (AllocaInst* A = dyn_cast<AllocaInst>(Lvar)) {
              string name = A->getName().str();
              int suffixIdx = name.find(".addr");
              if (suffixIdx != -1) {
                // it's an alloca for a param, now we find which one
                //dbgs() << "Name with suffix: " << name << "\n";
                name = name.substr(0, suffixIdx);
                //dbgs() << "Name without suffix: " << name << "\n";
                // find if there is an arg with the same name
                int i=0;
                for (Function::arg_iterator I = T->arg_begin(), E = T->arg_end(); I != E; I++) {
                  Argument* A2 = dyn_cast<Argument>(*&I);
                  if (A2->getName() == name) {
                    DEBUG(dbgs() << INDENT_1 << "Arg " << i << " has name " << A2->getName() << " (" << name << ")" << "\n");
                    break;
                  }
                  i++;
                }
                if (i < T->arg_size()) {
                  // we found the param index, propagate back to all caller args
                  for (CallInst* caller : CallGraphUtils::getCallers(T, M)) {
                    Value* arg = caller->getArgOperand(i);
                    DEBUG(dbgs() << INDENT_2 << "Adding arg " << *arg << " to worklist\n");
                    state[ContextUtils::SINGLE_CONTEXT][arg].insert(T);
                    addToWorklist(arg, ContextUtils::SINGLE_CONTEXT, worklist);
                  }
                }
              }
            }
          }
        }
      }
      else if (CallInst* C = dyn_cast<CallInst>(&*I)) { // passing functions as params
        if (Function* callee = CallGraphUtils::getDirectCallee(C)) {
          int argIdx = 0;
          for (Function::arg_iterator AI=callee->arg_begin(), AE=callee->arg_end(); AI != AE; AI++, argIdx++) {
            Value* Arg = C->getArgOperand(argIdx)->stripPointerCasts();
            Value* Param = &*AI;
            if (Function* T = dyn_cast<Function>(Arg)) {
              if (!T->isDeclaration()) {
                // we are assigning a function
                FunctionSet targets;
                targets.insert(T);
                //Arg->dump();
                //dbgs() << *Lvar << " = " << T->getName() << "()\n";
                //DEBUG(dbgs() << "Adding param " << *Param << " to worklist\n");
                state[ContextUtils::SINGLE_CONTEXT][Param] = targets;
                addToWorklist(Param, ContextUtils::SINGLE_CONTEXT, worklist);
              }
            }
          }
        }
      }
    }
  }

  DEBUG(dbgs() << "Globals:\n");
  // In some applications, functions are stored within globals aggregates like arrays
  // We search for such arrays conflating any structure contained within
  for (Module::global_iterator G = M.global_begin(), E = M.global_end(); G != E; ++G) {
    if (GlobalVariable* Gvar = dyn_cast<GlobalVariable>(G)) {
      //Gvar->dump();
      if (Gvar->hasInitializer()) {
        DEBUG(Gvar->getInitializer()->dump());
        if (ConstantArray* CA = dyn_cast<ConstantArray>(Gvar->getInitializer())) {
          DEBUG(dbgs() << "Constant array, num of operands: " << CA->getNumOperands() << "\n");
          for (int i=0; i<CA->getNumOperands(); i++) {
            Value* V = CA->getOperand(i);
            if (Function* F = dyn_cast<Function>(V->stripInBoundsOffsets())) {
              DEBUG(dbgs() << "Func: " << F->getName() << "\n");
              state[ContextUtils::SINGLE_CONTEXT][Gvar].insert(F);
              addToWorklist(Gvar, ContextUtils::SINGLE_CONTEXT, worklist);
            }
            else if (ConstantStruct* S = dyn_cast<ConstantStruct>(V->stripInBoundsOffsets())) {
              DEBUG(dbgs() << "Struct found within global " << Gvar->getName() << "\n");
              DEBUG(dbgs() << "Num operands: " << S->getNumOperands() << "\n");
              for (int j=0; j<S->getNumOperands(); j++) {
                Value* SMem = S->getOperand(j)->stripInBoundsOffsets();
                if (Function* F = dyn_cast<Function>(SMem)) {
                  DEBUG(dbgs() << "Func in struct: " << F->getName() << "\n");
                  state[ContextUtils::SINGLE_CONTEXT][Gvar].insert(F);
                  addToWorklist(Gvar, ContextUtils::SINGLE_CONTEXT, worklist);
                }
              }
            }
            else {
              //dbgs() << "Unknown element type\n";
              //V->dump();
            }
          }
        }
      }
    }
  }

  
}

void FPInferredTargetsAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  //llvm::DebugFlag = false;
}
