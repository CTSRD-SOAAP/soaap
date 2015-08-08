#include "Analysis/InfoFlow/SandboxPrivateAnalysis.h"
#include "Common/XO.h"
#include "Util/ContextUtils.h"
#include "Util/DebugUtils.h"
#include "Util/PrettyPrinters.h"
#include "Util/SandboxUtils.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/DebugInfo.h"
#include "soaap.h"

#include <random>

using namespace soaap;

/// Returns true if we should report a particular private access.
///
/// Uses the -soaap-privaccess-proportion command-line argument to make
/// a (potentially) stochastic decision.
static bool keepAccess();


void SandboxPrivateAnalysis::initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) {

  declassifierAnalysis.doAnalysis(M, sandboxes);

  int nextFreeIdx = -1;
 
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
            if (privSandboxIdxs != 0 and keepAccess()) {
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
              outputSources(ContextUtils::PRIV_CONTEXT, v, F);
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
              if (((privSandboxIdxs & ~name) != 0) && keepAccess()) {
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
                outputSources(S, v, F);
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
                  outputSources(S, rhs, F);
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
                    outputSources(S, arg, F);
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
                      outputSources(S, arg, F);
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
                      outputSources(S, arg, F);
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
                      outputSources(S, privateArg, F);
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
                    outputSources(S, retVal, F);
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

void SandboxPrivateAnalysis::outputSources(Context* C, Value* V, Function* F) {
  XO::List sourcesList("sources");
  int currIdx = 0;
  for (currIdx=0; currIdx<=31; currIdx++) {
    if ((state[C][V] & (1 << currIdx)) != 0) {
      Value* V2 = bitIdxToSource[currIdx];
      Function* sourceFunc = nullptr;
      XO::Instance sourcesInstance(sourcesList);
      XO::emit("{e:name/%s}", V2->getName().str().c_str());
      Instruction* I = nullptr;
      if (isa<IntrinsicInst>(V2)) {
        I = cast<IntrinsicInst>(V2);
      } else if (varToAnnotateCall.find(V2) != varToAnnotateCall.end()) {
        I = varToAnnotateCall[V2];
      }
      if (I) {
        sourceFunc = I->getParent()->getParent();
        PrettyPrinters::ppInstruction(I, false);
        // output trace from source to access
        if (funcToShortestCallPaths.find(sourceFunc) == funcToShortestCallPaths.end()) {
          calculateShortestCallPathsFromFunc(sourceFunc, C, (1 << currIdx));
        }
        InstTrace& callStack = funcToShortestCallPaths[sourceFunc][F];
        CallGraphUtils::emitCallTrace(callStack);
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

bool SandboxPrivateAnalysis::doesCallPropagateTaint(CallInst* C, int taint, Context* Ctx) {
  for (int argIdx=0; argIdx<C->getNumArgOperands(); argIdx++) {
    Value* arg = C->getArgOperand(argIdx);
    if ((state[Ctx][arg] & taint) != 0) {
      return true;
    }
  }
  return false;
}

void SandboxPrivateAnalysis::calculateShortestCallPathsFromFunc(Function* F, Context* Ctx, int taint) {
  // we use Dijkstra's algorithm
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "calculating shortest paths from " << F->getName() << " cache");

  // Find privileged path to instruction I, via a function that calls a sandboxed callee
  QueueSet<pair<Function*,Context*> > worklist;
  unordered_map<Function*,int> distanceFromMain;
  unordered_map<Function*,Function*> pred;
  unordered_map<Function*,CallInst*> call;

  worklist.enqueue(make_pair(F, Ctx));
  distanceFromMain[F] = 0;
  pred[F] = NULL;
  call[F] = NULL;

  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "setting all distances from main to INT_MAX\n")
  Module* M = F->getParent();
  for (Module::iterator F2 = M->begin(), E = M->end(); F2 != E; ++F2) {
    if (&*F2 != F) {
      distanceFromMain[&*F2] = INT_MAX;
    }
  }

  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "computing dijkstra's algorithm\n")
  while (!worklist.empty()) {
    pair<Function*,Context*> p = worklist.dequeue();
    Function* F2 = p.first;
    Context* Ctx2 = p.second;
    
    SDEBUG("soaap.util.callgraph", 4, dbgs() << INDENT_2 << "Current func: " << F2->getName() << "\n")
    // only proceed if:
    // a) in the privileged case, N is not a sandbox entrypoint
    // b) in the non-privileged case, if N is a sandbox entrypoint it must be S's
    // (i.e. in both case we are not entering a different protection domain)
    for (CallGraphEdge E : CallGraphUtils::getCallGraphEdges(F2, Ctx2, *M)) {
      CallInst* C = E.first;
      if (doesCallPropagateTaint(C, taint, Ctx2)) {
        Function* SuccFunc = E.second;
        Context* SuccCtx = Ctx2;
        if (SandboxUtils::isSandboxEntryPoint(*M, SuccFunc)) {
          SuccCtx = SandboxUtils::getSandboxForEntryPoint(SuccFunc, sandboxes);
        }
        else if (Sandbox* S = dyn_cast<Sandbox>(Ctx2)) {
          if (S->isCallgate(SuccFunc)) {
            SuccCtx = ContextUtils::PRIV_CONTEXT;
          }
        }
        // skip non-main root node
        SDEBUG("soaap.util.callgraph", 4, dbgs() << INDENT_3 << "Succ func: " << SuccFunc->getName() << "\n")
        if (distanceFromMain[SuccFunc] > distanceFromMain[F2]+1) {
          distanceFromMain[SuccFunc] = distanceFromMain[F2]+1;
          pred[SuccFunc] = F2;
          call[SuccFunc] = C;
          worklist.enqueue(make_pair(SuccFunc, SuccCtx));
        }
      }
    }
  }

  // cache shortest paths for each function
  SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "Caching shortest paths\n")
  for (Module::iterator F2 = M->begin(), E = M->end(); F2 != E; ++F2) {
    if (distanceFromMain[F2] < INT_MAX) { // N is reachable from main
      InstTrace path;
      Function* CurrF = &*F2;
      while (CurrF != F) {
        path.push_back(call[CurrF]);
        CurrF = pred[CurrF];
      }
      funcToShortestCallPaths[F][&*F2] = path;
      SDEBUG("soaap.util.callgraph", 3, dbgs() << INDENT_1 << "Paths from " << F->getName() << "() to " << F2->getName() << ": " << path.size() << "\n")
    }
  }

  SDEBUG("soaap.util.callgraph", 4, dbgs() << "completed calculating shortest paths from main cache\n");
}


static bool keepAccess() {
  static const double Probability = CmdLineOpts::PrivAccessProportion;
  static std::bernoulli_distribution dist(Probability);
  static std::default_random_engine generator;

  if (Probability == 0.0)
    return false;

  if (Probability == 1.0)
    return true;

  return dist(generator);
}
