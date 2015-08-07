#include "Analysis/InfoFlow/SandboxPrivateAnalysis.h"
#include "Common/XO.h"
#include "Util/ContextUtils.h"
#include "Util/DebugUtils.h"
#include "Util/PrettyPrinters.h"
#include "Util/SandboxUtils.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/DebugInfo.h"
#include "soaap.h"

using namespace soaap;

void SandboxPrivateAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {

  declassifierAnalysis.doAnalysis(M, sandboxes);

  int nextFreeIdx = 0;
 
  for (Sandbox* S : sandboxes) {
    int bitIdx = S->getNameIdx();
    ValueSet privateData = S->getPrivateData();
    for (Value* V : privateData) {
      if (IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(V)) {
        if (annotateCall->getIntrinsicID() == Intrinsic::var_annotation) {
          // llvm.var.annotation
          Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());
          varToAnnotateCall[annotatedVar] = annotateCall;
          bitIdxToSource[++nextFreeIdx] = annotatedVar;
          ContextVector Cs = ContextUtils::getContextsForMethod(annotateCall->getParent()->getParent(), contextInsensitive, sandboxes, M); 
          for (Context* C : Cs) {
            state[C][annotatedVar] |= (1 << nextFreeIdx);
            bitIdxToPrivSandboxIdxs[nextFreeIdx] |= (1 << bitIdx);
            addToWorklist(annotatedVar, C, worklist);
          }
        }
        else if (annotateCall->getIntrinsicID() == Intrinsic::ptr_annotation) {
          // llvm.ptr.annotation.p0i8
          bitIdxToSource[++nextFreeIdx] = annotateCall;
          bitIdxToPrivSandboxIdxs[nextFreeIdx] |= (1 << bitIdx);
          ContextVector Cs = ContextUtils::getContextsForMethod(annotateCall->getParent()->getParent(), contextInsensitive, sandboxes, M);
          for (Context* C : Cs) {
            addToWorklist(annotateCall, C, worklist);
            state[C][annotateCall] |= (1 << nextFreeIdx);
          }
        }
      }
      else if (GlobalVariable* G = dyn_cast<GlobalVariable>(V)) {
        bitIdxToSource[++nextFreeIdx] = G;
        bitIdxToPrivSandboxIdxs[nextFreeIdx] |= (1 << bitIdx);
        state[ContextUtils::NO_CONTEXT][G] |= (1 << nextFreeIdx);
        addToWorklist(G, ContextUtils::NO_CONTEXT, worklist);
      }
    }
  }

}

void SandboxPrivateAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  XO::List privateAccessList("private_access");
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
            int privSandboxIdxs = convertStateToBitIdxs(state[ContextUtils::PRIV_CONTEXT][v]);
            SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << "      Value:\n");
            SDEBUG("soaap.analysis.infoflow.private", 3, v->dump());
            SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << "      Value names: " << SandboxUtils::stringifySandboxNames(privSandboxIdxs) << "\n");
            if (privSandboxIdxs != 0) {
              XO::Instance privateAccessInstance(privateAccessList);
              XO::emit(" *** Privileged method \"{:function/%s}\" read data "
                       "value belonging to sandboxes: {d:sandboxes_private/%s}\n",
                       F->getName().str().c_str(),
                       SandboxUtils::stringifySandboxNames(privSandboxIdxs).c_str());
              XO::List sandboxPrivateList("sandbox_private");
              for (Sandbox* S : SandboxUtils::convertNamesToVector(privSandboxIdxs, sandboxes)) {
                XO::Instance sandboxPrivateInstance(sandboxPrivateList);
                XO::emit("{e:name/%s}", S->getName().c_str());
              }
              sandboxPrivateList.close();
              outputSources(ContextUtils::PRIV_CONTEXT, v);
              PrettyPrinters::ppInstruction(&I);
              if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                CallGraphUtils::emitCallTrace(F, NULL, M);
              }
              XO::emit("\n");
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
              int privSandboxIdxs = convertStateToBitIdxs(state[S][v]);
              SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << INDENT_3 << "Value: "; v->dump(););
              SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << INDENT_3 << "Private to sandboxes: " << SandboxUtils::stringifySandboxNames(privSandboxIdxs) << "\n");
              if (!(privSandboxIdxs == 0 || (privSandboxIdxs & name) == privSandboxIdxs)) {
                XO::Instance privateAccessInstance(privateAccessList);
                XO::emit(" *** Sandboxed method \"{:function/%s}\" read data "
                         "value belonging to sandboxes: {d:sandboxes_private/%s} "
                         "but it executes in sandboxes: {d:sandboxes_access/%s}\n",
                         F->getName().str().c_str(),
                         SandboxUtils::stringifySandboxNames(privSandboxIdxs).c_str(),
                         SandboxUtils::stringifySandboxNames(name).c_str());
                XO::List sandboxPrivateList("sandbox_private");
                for (Sandbox* S : SandboxUtils::convertNamesToVector(privSandboxIdxs, sandboxes)) {
                  XO::Instance sandboxPrivateInstance(sandboxPrivateList);
                  XO::emit("{e:name/%s}", S->getName().c_str());
                }
                sandboxPrivateList.close();
                XO::List sandboxAccessList("sandbox_access");
                for (Sandbox* S : SandboxUtils::convertNamesToVector(name, sandboxes)) {
                  XO::Instance sandboxAccessInstance(sandboxAccessList);
                  XO::emit("{e:name/%s}", S->getName().c_str());
                }
                sandboxPrivateList.close();
                outputSources(S, v);
                PrettyPrinters::ppInstruction(&I);
                if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                  CallGraphUtils::emitCallTrace(F, S, M);
                }
                XO::emit("\n");
              }
            }
          }
        }
      }
    }
  }
  privateAccessList.close();

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
  XO::List privateLeakList("private_leak");
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
                if (convertStateToBitIdxs(state[S][rhs]) & name) {
                  XO::Instance privateLeakInstance(privateLeakList);
                  XO::emit("{e:type/%s}", "global_var");
                  XO::emit(" *** Sandboxed method \"{:function/%s}\" executing "
                           "in sandboxes: {d:sandbox_access/%s} may leak "
                           "private data through global variable "
                           "{:var_name/%s}\n",
                  F->getName().str().c_str(),
                  SandboxUtils::stringifySandboxNames(name).c_str(),
                  gv->getName().str().c_str());
                  XO::List sandboxAccessList("sandbox_access");
                  for (Sandbox* S : SandboxUtils::convertNamesToVector(name, sandboxes)) {
                    XO::Instance sandboxAccessInstance(sandboxAccessList);
                    XO::emit("{e:name/%s}", S->getName().c_str());
                  }
                  sandboxAccessList.close();
                  outputSources(S, rhs);
                  PrettyPrinters::ppInstruction(&I);
                  if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                    CallGraphUtils::emitCallTrace(F, S, M);
                  }
                  XO::emit("\n");
                }
              }
            }
            else if (CallInst* call = dyn_cast<CallInst>(&I)) {
              // if this is a call to setenv, check the taint of the second argument
              if (Function* Callee = call->getCalledFunction()) {
                if (Callee->isIntrinsic()) continue;
                if (Callee->getName() == "setenv") {
                  Value* arg = call->getArgOperand(1);
                  if (convertStateToBitIdxs(state[S][arg]) & name) {
                    XO::Instance privateLeakInstance(privateLeakList);
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
                    XO::List sandboxAccessList("sandbox_access");
                    for (Sandbox* S : SandboxUtils::convertNamesToVector(name, sandboxes)) {
                      XO::Instance sandboxAccessInstance(sandboxAccessList);
                      XO::emit("{e:name/%s}", S->getName().c_str());
                    }
                    sandboxAccessList.close();
                    outputSources(S, arg);
                    PrettyPrinters::ppInstruction(&I);
                    if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                      CallGraphUtils::emitCallTrace(F, S, M);
                    }
                    XO::emit("\n");
                  }
                }
                else if (Callee->getBasicBlockList().empty()) {
                  // extern function
                  SDEBUG("soaap.analysis.infoflow.private", 3, dbgs() << "Extern callee: " << Callee->getName() << "\n");
                  for (User::op_iterator AI=call->op_begin(), AE=call->op_end(); AI!=AE; AI++) {
                    Value* arg = dyn_cast<Value>(AI->get());
                    if (convertStateToBitIdxs(state[S][arg]) & name) {
                      XO::Instance privateLeakInstance(privateLeakList);
                      XO::emit("{e:type/%s}", "extern");
                      XO::emit(" *** Sandboxed method \"{:function}\" executing "
                               "in sandboxes: {d:sandboxes/%s} may leak private "
                               "data through the extern function \"{:callee/%s}\"\n",
                               F->getName().str().c_str(),
                               SandboxUtils::stringifySandboxNames(name).c_str(),
                               Callee->getName().str().c_str());
                      XO::List sandboxAccessList("sandbox_access");
                      for (Sandbox* S : SandboxUtils::convertNamesToVector(name, sandboxes)) {
                        XO::Instance sandboxAccessInstance(sandboxAccessList);
                        XO::emit("{e:name/%s}", S->getName().c_str());
                      }
                      sandboxAccessList.close();
                      outputSources(S, arg);
                      PrettyPrinters::ppInstruction(&I);
                      if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                        CallGraphUtils::emitCallTrace(F, S, M);
                      }
                      XO::emit("\n");
                    }
                  }
                }
                else if (find(callgates.begin(), callgates.end(), Callee) != callgates.end()) {
                  // cross-domain call to callgate
                  for (User::op_iterator AI=call->op_begin(), AE=call->op_end(); AI!=AE; AI++) {
                    Value* arg = dyn_cast<Value>(AI->get());
                    if (convertStateToBitIdxs(state[S][arg]) & name) {
                      XO::Instance privateLeakInstance(privateLeakList);
                      XO::emit("{e:type/%s}", "callgate");
                      XO::emit(" *** Sandboxed method \"{:function}\" executing "
                               "in sandboxes: {d:sandboxes/%s} may leak private "
                               "data through callgate \"{:callgate/%s}\"\n",
                               F->getName().str().c_str(),
                               SandboxUtils::stringifySandboxNames(name).c_str(),
                               Callee->getName().str().c_str());
                      outputSources(S, arg);
                      PrettyPrinters::ppInstruction(&I);
                      if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                        CallGraphUtils::emitCallTrace(F, S, M);
                      }
                      XO::emit("\n");
                    }
                  }
                  XO::emit("\n");
                }
                else if (SandboxUtils::isSandboxEntryPoint(M, Callee)) { // possible cross-sandbox call
                  Sandbox* S2 = SandboxUtils::getSandboxForEntryPoint(Callee, sandboxes);
                  if (S != S2) {
                    Value* privateArg = nullptr;
                    for (int i=0; i<call->getNumArgOperands(); i++) {
                      Value* arg = call->getArgOperand(i);
                      if (convertStateToBitIdxs(state[S][arg]) & name) {
                        privateArg = arg;
                        break;
                      }
                    }
                    
                    if (privateArg) {
                      XO::Instance privateLeakInstance(privateLeakList);
                      XO::emit("{e:type/%s}", "cross_sandbox");
                      XO::emit(" *** Sandboxed method \"{:function}\" executing "
                               "in sandboxes: {d:sandboxes/%s} may leak private "
                               "data through a cross-sandbox call into "
                               "[{:callee_sandbox/%s}]\n",
                               F->getName().str().c_str(),
                               SandboxUtils::stringifySandboxNames(name).c_str(),
                               S2->getName().c_str());
                      XO::List sandboxAccessList("sandbox_access");
                      for (Sandbox* S : SandboxUtils::convertNamesToVector(name, sandboxes)) {
                        XO::Instance sandboxAccessInstance(sandboxAccessList);
                        XO::emit("{e:name/%s}", S->getName().c_str());
                      }
                      sandboxAccessList.close();
                      outputSources(S, privateArg);
                      PrettyPrinters::ppInstruction(&I);
                      if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                        CallGraphUtils::emitCallTrace(F, S, M);
                      }
                      XO::emit("\n");
                    }
                  }
                }
              }
            }
            else if (ReturnInst* ret = dyn_cast<ReturnInst>(&I)) {
              // we are returning from the sandbox entrypoint function
              if (S->isEntryPoint(F)) {
                if (Value* retVal = ret->getReturnValue()) {
                  if (convertStateToBitIdxs(state[S][retVal]) & name) {
                    XO::Instance privateLeakInstance(privateLeakList);
                    XO::emit("{e:type/%s}", "return_from_entrypoint");
                    XO::emit(" *** Sandbox \"{:sandbox/%s}\" "
                             "may leak private data when returning a value "
                             "from entrypoint \"{:entrypoint/%s}\"\n",
                             S->getName().c_str(),
                             F->getName().str().c_str());
                    outputSources(S, retVal);
                    PrettyPrinters::ppInstruction(&I);
                    if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::OutputTraces)) {
                      CallGraphUtils::emitCallTrace(F, S, M);
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
  privateLeakList.close();
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
  int privSandboxIdxs = 0;
  int currIdx = 0;
  for (currIdx=0; currIdx<=31; currIdx++) {
    if (fact & (1 << currIdx)) {
      privSandboxIdxs |= bitIdxToPrivSandboxIdxs[currIdx];
    }
  }
  return SandboxUtils::stringifySandboxNames(privSandboxIdxs);
}

bool SandboxPrivateAnalysis::checkEqual(int f1, int f2) {
  return f1 == f2;
}

int SandboxPrivateAnalysis::convertStateToBitIdxs(int& vs) {
  int privSandboxIdxs = 0;
  int currIdx = 0;
  for (currIdx=0; currIdx<=31; currIdx++) {
    if (vs & (1 << currIdx)) {
      privSandboxIdxs |= bitIdxToPrivSandboxIdxs[currIdx];
    }
  }
  return privSandboxIdxs;
}

void SandboxPrivateAnalysis::outputSources(Context* C, Value* V) {
  XO::List sourcesList("sources");
  int currIdx = 0;
  for (currIdx=0; currIdx<=31; currIdx++) {
    if (state[C][V] & (1 << currIdx)) {
      Value* V2 = bitIdxToSource[currIdx];
      XO::Instance sourcesInstance(sourcesList);
      XO::emit("{e:name/%s}", V2->getName().str().c_str());
      Instruction* I = nullptr;
      if (isa<IntrinsicInst>(V2)) {
        I = cast<IntrinsicInst>(V2);
      } else if (varToAnnotateCall.find(V2) != varToAnnotateCall.end()) {
        I = varToAnnotateCall[V2];
      }
      if (I) {
        PrettyPrinters::ppInstruction(I, false);
      }
      else {
        // global variable
        GlobalVariable* G = cast<GlobalVariable>(V2);
        pair<string,int> declareLoc = DebugUtils::findGlobalDeclaration(G);
        XO::Container globalLoc("location");
        XO::emit("{e:file/%s}{e:line/%d}",declareLoc.first.c_str(), declareLoc.second);
      }
    }
  }
}
