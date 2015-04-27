#include "Analysis/InfoFlow/SandboxPrivateAnalysis.h"
#include "Common/XO.h"
#include "Util/ContextUtils.h"
#include "Util/DebugUtils.h"
#include "Util/InstUtils.h"
#include "Util/SandboxUtils.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/DebugInfo.h"
#include "soaap.h"

using namespace soaap;

void SandboxPrivateAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {

  declassifierAnalysis.doAnalysis(M, sandboxes);
 
  for (Sandbox* S : sandboxes) {
    int bitIdx = S->getNameIdx();
    ValueSet privateData = S->getPrivateData();
    for (Value* V : privateData) {
      if (IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(V)) {
        if (annotateCall->getIntrinsicID() == Intrinsic::var_annotation) {
          // llvm.var.annotation
          Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());
          ContextVector Cs = ContextUtils::getContextsForMethod(annotateCall->getParent()->getParent(), contextInsensitive, sandboxes, M); 
          for (Context* C : Cs) {
            state[C][annotatedVar] |= (1 << bitIdx);
            addToWorklist(annotatedVar, C, worklist);
          }
        }
        else if (annotateCall->getIntrinsicID() == Intrinsic::ptr_annotation) {
          // llvm.ptr.annotation.p0i8
          ContextVector Cs = ContextUtils::getContextsForMethod(annotateCall->getParent()->getParent(), contextInsensitive, sandboxes, M);
          for (Context* C : Cs) {
            addToWorklist(annotateCall, C, worklist);
            state[C][annotateCall] |= (1 << bitIdx);
          }
        }
      }
      else if (GlobalVariable* G = dyn_cast<GlobalVariable>(V)) {
        state[ContextUtils::NO_CONTEXT][G] |= (1 << bitIdx);
        addToWorklist(G, ContextUtils::NO_CONTEXT, worklist);
      }
    }
  }

}

void SandboxPrivateAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  XO::open_list("private_access");
  // validate that sandbox-private data is never accessed in other contexts
  for (Function* F : privilegedMethods) {
    if (shouldOutputWarningFor(F)) {
      for (BasicBlock& BB : F->getBasicBlockList()) {
        for (Instruction& I : BB.getInstList()) {
          SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << "   Instruction:\n");
          SDEBUG("soaap.analysis.infoflow.private", 3, I.dump());
          LoadInst* load2 = dyn_cast<LoadInst>(&I);
          if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
            Value* v = load->getPointerOperand()->stripPointerCasts();
            SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << "      Value:\n");
            SDEBUG("soaap.analysis.infoflow.private", 3, v->dump());
            SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << "      Value names: " << state[ContextUtils::PRIV_CONTEXT][v] << ", " << SandboxUtils::stringifySandboxNames(state[ContextUtils::PRIV_CONTEXT][v]) << "\n");
            if (state[ContextUtils::PRIV_CONTEXT][v] != 0) {
              XO::open_instance("private_access");
              XO::emit(" *** Privileged method \"{:function/%s}\" read data "
                       "value belonging to sandboxes: {d:sandboxes_private/%s}\n",
                       F->getName().str().c_str(),
                       SandboxUtils::stringifySandboxNames(state[ContextUtils::PRIV_CONTEXT][v]).c_str());
              XO::open_list("sandbox_private");
              for (Sandbox* S : SandboxUtils::convertNamesToVector(state[ContextUtils::PRIV_CONTEXT][v], sandboxes)) {
                XO::open_instance("sandbox_private");
                XO::emit("{e:name/%s}", S->getName().c_str());
                XO::close_instance("sandbox_private");
              }
              XO::close_list("sandbox_private");
              InstUtils::emitInstLocation(&I);
              if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                CallGraphUtils::emitCallTrace(F, NULL, M);
              }
              XO::emit("\n");
              XO::close_instance("private_access");
            }
          }
        }
      }
    }
  }

  // check sandboxes
  for (Sandbox* S : sandboxes) {
    FunctionVector sandboxedFuncs = S->getFunctions();
    int name = 1 << S->getNameIdx();
    for (Function* F : sandboxedFuncs) {
      if (shouldOutputWarningFor(F)) {
        SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << INDENT_1 << "Function: " << F->getName() << "\n");
        for (BasicBlock& BB : F->getBasicBlockList()) {
          for (Instruction& I : BB.getInstList()) {
            SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << INDENT_2 << "Instruction: "; I.dump(););
            LoadInst* load2 = dyn_cast<LoadInst>(&I);
            if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
              Value* v = load->getPointerOperand()->stripPointerCasts();
              SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << INDENT_3 << "Value: "; v->dump(););
              SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << INDENT_3 << "Value names: " << state[S][v] << ", " << SandboxUtils::stringifySandboxNames(state[S][v]) << "\n");
              if (!(state[S][v] == 0 || (state[S][v] & name) == state[S][v])) {
                XO::open_instance("private_access");
                XO::emit(" *** Sandboxed method \"{:function/%s}\" read data "
                         "value belonging to sandboxes: {d:sandboxes_private/%s} "
                         "but it executes in sandboxes: {d:sandboxes_access/%s}\n",
                         F->getName().str().c_str(),
                         SandboxUtils::stringifySandboxNames(state[S][v]).c_str(),
                         SandboxUtils::stringifySandboxNames(name).c_str());
                XO::open_list("sandbox_private");
                for (Sandbox* S : SandboxUtils::convertNamesToVector(state[S][v], sandboxes)) {
                  XO::open_instance("sandbox_private");
                  XO::emit("{e:name/%s}", S->getName().c_str());
                  XO::close_instance("sandbox_private");
                }
                XO::close_list("sandbox_private");
                XO::open_list("sandbox_access");
                for (Sandbox* S : SandboxUtils::convertNamesToVector(name, sandboxes)) {
                  XO::open_instance("sandbox_access");
                  XO::emit("{e:name/%s}", S->getName().c_str());
                  XO::close_instance("sandbox_access");
                }
                XO::close_list("sandbox_access");
                InstUtils::emitInstLocation(&I);
                if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                  CallGraphUtils::emitCallTrace(F, S, M);
                }
                XO::emit("\n");
                XO::close_instance("private_access");
              }
            }
          }
        }
      }
    }
  }
  XO::close_list("private_access");

  // Validate that data cannot leak out of a sandbox.
  // Currently, SOAAP looks for escapement via:
  //   1) Assignments to shared global variables.
  //   2) Arguments to functions for which there is no body (due to incomplete call graph).
  //   3) Arguments to functions of callgates.
  //   4) Arguments to functions that are executed in a different sandbox
  //      (i.e. cross-domain calls).
  //   5) Assignments to environment variables.
  //   6) Arguments to system calls.
  //   7) Return from the sandbox entrypoint.
  XO::open_list("private_leak");
  for (Sandbox* S : sandboxes) {
    FunctionVector sandboxedFuncs = S->getFunctions();
    FunctionVector callgates = S->getCallgates();
    int name = 1 << S->getNameIdx();
    for (Function* F : sandboxedFuncs) {
      if (shouldOutputWarningFor(F)) {
        SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << INDENT_1 << "Function: " << F->getName());
        SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << ", sandbox names: " << SandboxUtils::stringifySandboxNames(name) << "\n");
        for (BasicBlock& BB : F->getBasicBlockList()) {
          for (Instruction& I : BB.getInstList()) {
            SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << INDENT_2 << "Instruction: "; I.dump(););
            // if assignment to a global variable, then check taint of value
            // being assigned
            if (StoreInst* store = dyn_cast<StoreInst>(&I)) {
              Value* lhs = store->getPointerOperand();
              if (GlobalVariable* gv = dyn_cast<GlobalVariable>(lhs)) {
                Value* rhs = store->getValueOperand();
                // if the rhs is private to the current sandbox, then flag an error
                if (state[S][rhs] & name) {
                  XO::open_instance("private_leak");
                  XO::emit("{e:type/%s}", "global_var");
                  XO::emit(" *** Sandboxed method \"{:function/%s}\" executing "
                           "in sandboxes: {d:sandbox_access/%s} may leak "
                           "private data through global variable "
                           "{:var_name/%s}\n",
                  F->getName().str().c_str(),
                  SandboxUtils::stringifySandboxNames(name).c_str(),
                  gv->getName().str().c_str());
                  XO::open_list("sandbox_access");
                  for (Sandbox* S : SandboxUtils::convertNamesToVector(name, sandboxes)) {
                    XO::open_instance("sandbox_access");
                    XO::emit("{e:name/%s}", S->getName().c_str());
                    XO::close_instance("sandbox_access");
                  }
                  XO::close_list("sandbox_access");
                  InstUtils::emitInstLocation(&I);
                  if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                    CallGraphUtils::emitCallTrace(F, S, M);
                  }
                  XO::emit("\n");
                  XO::close_instance("private_leak");
                }
              }
            }
            else if (CallInst* call = dyn_cast<CallInst>(&I)) {
              // if this is a call to setenv, check the taint of the second argument
              if (Function* Callee = call->getCalledFunction()) {
                if (Callee->isIntrinsic()) continue;
                if (Callee->getName() == "setenv") {
                  Value* arg = call->getArgOperand(1);
                  if (state[S][arg] & name) {
                    XO::open_instance("private_leak");
                    XO::emit("{e:type/%s}", "env_var");
                    XO::emit(" *** Sandboxed method \"{:function}\" executing "
                             "in sandboxes: {d:sandboxes/%s} may leak private "
                             "data through env var ",
                             F->getName().str().c_str(),
                             SandboxUtils::stringifySandboxNames(name).c_str());
                    if (GlobalVariable* envVarGlobal = dyn_cast<GlobalVariable>(call->getArgOperand(0)->stripPointerCasts())) {
                      ConstantDataArray* envVarArray = dyn_cast<ConstantDataArray>(envVarGlobal->getInitializer());
                      string envVarName = envVarArray->getAsString();
                      XO::emit("\"{:env_var/%s}\"", envVarName.c_str());
                    }
                    XO::emit("\n");
                    XO::open_list("sandbox_access");
                    for (Sandbox* S : SandboxUtils::convertNamesToVector(name, sandboxes)) {
                      XO::open_instance("sandbox_access");
                      XO::emit("{e:name/%s}", S->getName().c_str());
                      XO::close_instance("sandbox_access");
                    }
                    XO::close_list("sandbox_access");
                    InstUtils::emitInstLocation(&I);
                    if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                      CallGraphUtils::emitCallTrace(F, S, M);
                    }
                    XO::emit("\n");
                    XO::close_instance("private_leak");
                  }
                }
                else if (Callee->getBasicBlockList().empty()) {
                  // extern function
                  SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << "Extern callee: " << Callee->getName() << "\n");
                  for (User::op_iterator AI=call->op_begin(), AE=call->op_end(); AI!=AE; AI++) {
                    Value* arg = dyn_cast<Value>(AI->get());
                    if (state[S][arg] & name) {
                      XO::open_instance("private_leak");
                      XO::emit("{e:type/%s}", "extern");
                      XO::emit(" *** Sandboxed method \"{:function}\" executing "
                               "in sandboxes: {d:sandboxes/%s} may leak private "
                               "data through the extern function \"{:callee/%s}\"\n",
                               F->getName().str().c_str(),
                               SandboxUtils::stringifySandboxNames(name).c_str(),
                               Callee->getName().str().c_str());
                      XO::open_list("sandbox_access");
                      for (Sandbox* S : SandboxUtils::convertNamesToVector(name, sandboxes)) {
                        XO::open_instance("sandbox_access");
                        XO::emit("{e:name/%s}", S->getName().c_str());
                        XO::close_instance("sandbox_access");
                      }
                      XO::close_list("sandbox_access");
                      InstUtils::emitInstLocation(&I);
                      if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                        CallGraphUtils::emitCallTrace(F, S, M);
                      }
                      XO::emit("\n");
                      XO::close_instance("private_leak");
                    }
                  }
                }
                else if (find(callgates.begin(), callgates.end(), Callee) != callgates.end()) {
                  // cross-domain call to callgate
                  for (User::op_iterator AI=call->op_begin(), AE=call->op_end(); AI!=AE; AI++) {
                    Value* arg = dyn_cast<Value>(AI->get());
                    if (state[S][arg] & name) {
                      XO::open_instance("private_leak");
                      XO::emit("{e:type/%s}", "callgate");
                      XO::emit(" *** Sandboxed method \"{:function}\" executing "
                               "in sandboxes: {d:sandboxes/%s} may leak private "
                               "data through callgate \"{:callgate/%s}\"\n",
                               F->getName().str().c_str(),
                               SandboxUtils::stringifySandboxNames(name).c_str(),
                               Callee->getName().str().c_str());
                      InstUtils::emitInstLocation(&I);
                      if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                        CallGraphUtils::emitCallTrace(F, S, M);
                      }
                      XO::emit("\n");
                      XO::close_instance("private_leak");
                    }
                  }
                  XO::emit("\n");
                }
                else if (SandboxUtils::isSandboxEntryPoint(M, Callee)) { // possible cross-sandbox call
                  Sandbox* S2 = SandboxUtils::getSandboxForEntryPoint(Callee, sandboxes);
                  if (S != S2) {
                    bool privateArg = false;
                    for (int i=0; i<call->getNumArgOperands(); i++) {
                      Value* arg = call->getArgOperand(i);
                      if (state[S][arg] & name) {
                        privateArg = true;
                        break;
                      }
                    }
                    
                    if (privateArg) {
                      XO::open_instance("private_leak");
                      XO::emit("{e:type/%s}", "cross_sandbox");
                      XO::emit(" *** Sandboxed method \"{:function}\" executing "
                               "in sandboxes: {d:sandboxes/%s} may leak private "
                               "data through a cross-sandbox call into "
                               "[{:callee_sandbox/%s}]\n",
                               F->getName().str().c_str(),
                               SandboxUtils::stringifySandboxNames(name).c_str(),
                               S2->getName().c_str());
                      XO::open_list("sandbox_access");
                      for (Sandbox* S : SandboxUtils::convertNamesToVector(name, sandboxes)) {
                        XO::open_instance("sandbox_access");
                        XO::emit("{e:name/%s}", S->getName().c_str());
                        XO::close_instance("sandbox_access");
                      }
                      XO::close_list("sandbox_access");
                      InstUtils::emitInstLocation(&I);
                      if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                        CallGraphUtils::emitCallTrace(F, S, M);
                      }
                      XO::emit("\n");
                      XO::close_instance("private_leak");
                    }
                  }
                }
              }
            }
            else if (ReturnInst* ret = dyn_cast<ReturnInst>(&I)) {
              // we are returning from the sandbox entrypoint function
              if (S->isEntryPoint(F)) {
                if (Value* retVal = ret->getReturnValue()) {
                  if (state[S][retVal] & name) {
                    XO::open_instance("private_leak");
                    XO::emit("{e:type/%s}", "return_from_entrypoint");
                    XO::emit(" *** Sandbox \"{:sandbox/%s}\" "
                             "may leak private data when returning a value "
                             "from entrypoint \"{:entrypoint/%s}\"\n",
                             S->getName().c_str(),
                             F->getName().str().c_str());
                    InstUtils::emitInstLocation(&I);
                    if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                      CallGraphUtils::emitCallTrace(F, S, M);
                    }
                    XO::close_instance("private_leak");
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  XO::close_list("private_leak");
}

bool SandboxPrivateAnalysis::propagateToValue(const Value* from, const Value* to, Context* cFrom, Context* cTo, Module& M) {
  if (!declassifierAnalysis.isDeclassified(from)) {
    return InfoFlowAnalysis<int>::propagateToValue(from, to, cFrom, cTo, M);
  }
  return false;
}

bool SandboxPrivateAnalysis::performMeet(int from, int& to) {
  return performUnion(from, to);
}

bool SandboxPrivateAnalysis::performUnion(int from, int& to) {
  int oldTo = to;
  to = from | to;
  return to != oldTo;
}

string SandboxPrivateAnalysis::stringifyFact(int fact) {
  return SandboxUtils::stringifySandboxNames(fact);
}
