#include "Analysis/InfoFlow/SandboxPrivateAnalysis.h"
#include "Util/SandboxUtils.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/DebugInfo.h"
#include "soaap.h"

using namespace soaap;

void SandboxPrivateAnalysis::initialise(ValueList& worklist, Module& M, SandboxVector& sandboxes) {
  // initialise with pointers to annotated fields and uses of annotated global variables
  if (Function* F = M.getFunction("llvm.ptr.annotation.p0i8")) {
    for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
      User* user = u.getUse().getUser();
      if (isa<IntrinsicInst>(user)) {
        IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(user);
        Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());

        GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
        ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
        StringRef annotationStrValCString = annotationStrValArray->getAsCString();
        
        if (annotationStrValCString.startswith(SANDBOX_PRIVATE)) {
          StringRef sandboxName = annotationStrValCString.substr(strlen(SANDBOX_PRIVATE)+1); //+1 because of _
          SandboxUtils::assignBitIdxToSandboxName(sandboxName);
          int bitIdx = SandboxUtils::getBitIdxFromSandboxName(sandboxName);
        
          DEBUG(dbgs() << "   Sandbox-private annotation " << annotationStrValCString << " found:\n");
        
          worklist.push_back(annotatedVar);
          state[annotateCall] |= (1 << bitIdx);
        }
      }
    }
  }
  
  if (Function* F = M.getFunction("llvm.var.annotation")) {
    for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
      User* user = u.getUse().getUser();
      if (isa<IntrinsicInst>(user)) {
        IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(user);
        Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());

        GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
        ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
        StringRef annotationStrValCString = annotationStrValArray->getAsCString();
        if (annotationStrValCString.startswith(SANDBOX_PRIVATE)) {
          StringRef sandboxName = annotationStrValCString.substr(strlen(SANDBOX_PRIVATE)+1); //+1 because of _
          SandboxUtils::assignBitIdxToSandboxName(sandboxName);
          int bitIdx = SandboxUtils::getBitIdxFromSandboxName(sandboxName);
        
          DEBUG(dbgs() << "   Sandbox-private annotation " << annotationStrValCString << " found:\n");
          DEBUG(annotatedVar->dump());
          worklist.push_back(annotatedVar);
          state[annotatedVar] |= (1 << bitIdx);
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
      if (annotationStrArrayCString.startswith(SANDBOX_PRIVATE)) {
        GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
        if (isa<GlobalVariable>(annotatedVal)) {
          GlobalVariable* annotatedVar = dyn_cast<GlobalVariable>(annotatedVal);
          if (annotationStrArrayCString.startswith(SANDBOX_PRIVATE)) {
            StringRef sandboxName = annotationStrArrayCString.substr(strlen(SANDBOX_PRIVATE)+1);
            DEBUG(dbgs() << "    Found sandbox-private global variable " << annotatedVar->getName() << "; belongs to \"" << sandboxName << "\"\n");
            SandboxUtils::assignBitIdxToSandboxName(sandboxName);
            state[annotatedVar] |= (1 << SandboxUtils::getBitIdxFromSandboxName(sandboxName));
          }
        }
      }
    }
  }

}

void SandboxPrivateAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
  // validate that sandbox-private data is never accessed in other sandboxed contexts
  for (Function* F : privilegedMethods) {
    for (BasicBlock& BB : F->getBasicBlockList()) {
      for (Instruction& I : BB.getInstList()) {
        DEBUG(dbgs() << "   Instruction:\n");
        DEBUG(I.dump());
        LoadInst* load2 = dyn_cast<LoadInst>(&I);
        if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
          Value* v = load->getPointerOperand()->stripPointerCasts();
          DEBUG(dbgs() << "      Value:\n");
          DEBUG(v->dump());
          DEBUG(dbgs() << "      Value names: " << state[v] << ", " << SandboxUtils::stringifySandboxNames(state[v]) << "\n");
          if (state[v] != 0) {
            outs() << " *** Privileged method " << F->getName() << " read data value private to sandboxes: " << SandboxUtils::stringifySandboxNames(state[v]) << "\n";
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

  // check sandboxes
  FunctionIntMap sandboxEntryPointToName;
  for (Sandbox* S : sandboxes) {
    FunctionVector sandboxedFuncs = S->getFunctions();
    int name = 1 << S->getNameIdx();
    Function* entryPoint = S->getEntryPoint();
    sandboxEntryPointToName[entryPoint] = name;
    for (Function* F : sandboxedFuncs) {
      DEBUG(dbgs() << "Function: " << F->getName());
      for (BasicBlock& BB : F->getBasicBlockList()) {
        for (Instruction& I : BB.getInstList()) {
          DEBUG(dbgs() << "   Instruction:\n");
          DEBUG(I.dump());
          LoadInst* load2 = dyn_cast<LoadInst>(&I);
          if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
            Value* v = load->getPointerOperand()->stripPointerCasts();
            DEBUG(dbgs() << "      Value:\n");
            DEBUG(v->dump());
            DEBUG(dbgs() << "      Value names: " << state[v] << ", " << SandboxUtils::stringifySandboxNames(state[v]) << "\n");
            if (!(state[v] == 0 || (state[v] & name) == state[v])) {
              outs() << " *** Sandboxed method \"" << F->getName() << "\" read data value belonging to sandboxes: " << SandboxUtils::stringifySandboxNames(state[v]) << " but it executes in sandboxes: " << SandboxUtils::stringifySandboxNames(name) << "\n";
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

  // Validate that data cannot leak out of a sandbox.
  // Currently, SOAAP looks for escapement via:
  //   1) Assignments to shared global variables.
  //   2) Arguments to functions for which there is no body (due to incomplete call graph).
  //   3) Arguments to functions of callgates.
  //   4) Arguments to functions that are executed in a different sandbox
  //      (i.e. cross-domain calls).
  //   5) Assignments to environment variables.
  //   6) Arguments to system calls
  for (Sandbox* S : sandboxes) {
    FunctionVector sandboxedFuncs = S->getFunctions();
    FunctionVector callgates = S->getCallgates();
    int name = 1 << S->getNameIdx();
    for (Function* F : sandboxedFuncs) {
      DEBUG(dbgs() << "Function: " << F->getName());
      DEBUG(dbgs() << ", sandbox names: " << SandboxUtils::stringifySandboxNames(name) << "\n");
      for (BasicBlock& BB : F->getBasicBlockList()) {
        for (Instruction& I : BB.getInstList()) {
          DEBUG(dbgs() << "   Instruction:\n");
          DEBUG(I.dump());
          // if assignment to a global variable, then check taint of value
          // being assigned
          if (StoreInst* store = dyn_cast<StoreInst>(&I)) {
            Value* lhs = store->getPointerOperand();
            if (GlobalVariable* gv = dyn_cast<GlobalVariable>(lhs)) {
              Value* rhs = store->getValueOperand();
              // if the rhs is private to the current sandbox, then flag an error
              if (state[rhs] & name) {
                outs() << " *** Sandboxed method \"" << F->getName() << "\" executing in sandboxes: " << SandboxUtils::stringifySandboxNames(name) << " may leak private data through global variable " << gv->getName() << "\n";
                if (MDNode *N = I.getMetadata("dbg")) {
                  DILocation loc(N);
                  outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                }
                outs() << "\n";
              }
            }
          }
          else if (CallInst* call = dyn_cast<CallInst>(&I)) {
            // if this is a call to setenv, check the taint of the second argument
            if (Function* Callee = call->getCalledFunction()) {
              if (Callee->isIntrinsic()) continue;
              if (Callee->getName() == "setenv") {
                Value* arg = call->getArgOperand(1);
              
                if (state[arg] & name) {
                  outs() << " *** Sandboxed method \"" << F->getName() << "\" executing in sandboxes: " << SandboxUtils::stringifySandboxNames(name) << " may leak private data through env var ";
                  if (GlobalVariable* envVarGlobal = dyn_cast<GlobalVariable>(call->getArgOperand(0)->stripPointerCasts())) {
                    ConstantDataArray* envVarArray = dyn_cast<ConstantDataArray>(envVarGlobal->getInitializer());
                    StringRef envVarName = envVarArray->getAsString();
                    outs() << "\"" << envVarName << "\"";
                  }
                  outs() << "\n";
                  if (MDNode *N = I.getMetadata("dbg")) {
                    DILocation loc(N);
                    outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                  }
                  outs() << "\n";
                }
              }
              else if (Callee->getBasicBlockList().empty()) {
                // extern function
                DEBUG(dbgs() << "Extern callee: " << Callee->getName() << "\n");
                for (User::op_iterator AI=call->op_begin(), AE=call->op_end(); AI!=AE; AI++) {
                  Value* arg = dyn_cast<Value>(AI->get());
                  if (state[arg] & name) {
                    outs() << " *** Sandboxed method \"" << F->getName() << "\" executing in sandboxes: " <<      SandboxUtils::stringifySandboxNames(name) << " may leak private data through the extern function " << Callee->getName() << "\n";
                    if (MDNode *N = I.getMetadata("dbg")) {
                      DILocation loc(N);
                      outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                    }
                    outs() << "\n";
                  }
                }
              }
              else if (find(callgates.begin(), callgates.end(), Callee) != callgates.end()) {
                // cross-domain call to callgate
                for (User::op_iterator AI=call->op_begin(), AE=call->op_end(); AI!=AE; AI++) {
                  Value* arg = dyn_cast<Value>(AI->get());
                  if (state[arg] & name) {
                    outs() << " *** Sandboxed method \"" << F->getName() << "\" executing in sandboxes: " <<      SandboxUtils::stringifySandboxNames(name) << " may leak private data through callgate " << Callee->getName() << "\n";
                    if (MDNode *N = I.getMetadata("dbg")) {
                      DILocation loc(N);
                      outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                    }
                    outs() << "\n";
                  }
                }
                outs() << "\n";
              }
              else if (SandboxUtils::isSandboxEntryPoint(M, Callee)) { // possible cross-sandbox call
                outs() << " *** Sandboxed method \"" << F->getName() << "\" executing in sandboxes: " <<      SandboxUtils::stringifySandboxNames(name) << " may leak private data through a cross-sandbox call into: " << SandboxUtils::stringifySandboxNames(sandboxEntryPointToName[Callee]) << "\n";
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
}
