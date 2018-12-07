/*
 * Copyright (c) 2013-2015 Khilan Gudka
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "Common/Debug.h"
#include "Common/Sandbox.h"
#include "Util/CallGraphUtils.h"
#include "Util/ContextUtils.h"
#include "Util/DebugUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/PrettyPrinters.h"
#include "Util/SandboxUtils.h"
#include "soaap.h"
#define IN_SOAAP_GENERATOR
#include "soaap_gen.h"

#include "llvm/IR/CFG.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Transforms/Utils/Local.h"

#include <sstream>

using namespace soaap;

Sandbox::Sandbox(string n, int i, FunctionSet entries, bool p, Module& m, int o, int c,
    FunctionSet internalFunctions, ValueSet internalGlobals) 
  : Context(CK_SANDBOX), name(n), nameIdx(i), entryPoints(entries), persistent(p), module(m), overhead(o), clearances(c), internalFuncs(internalFunctions), internalGlobs(internalGlobals) {
}

Sandbox::Sandbox(string n, int i, InstVector& r, bool p, Module& m) 
  : Context(CK_SANDBOX), name(n), nameIdx(i), region(r), persistent(p), module(m), overhead(0), clearances(0) {
}

void Sandbox::init() {
	SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Finding sandboxed functions\n");
  findSandboxedFunctions();
  SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Finding sandboxed calls\n");
  findSandboxedCalls();
	SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Finding shared global variables\n");
  findSharedGlobalVariables();
	SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Finding callgates\n");
  findCallgates();
  if (!entryPoints.empty()) {
	  SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Finding capabilities\n");
    findCapabilities();
  }
  SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Finding creation points\n");
  findCreationPoints();
  SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Finding annotated inputs\n");
  findAnnotatedInputs();
  SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Finding object inputs\n");
  findObjectInputTypes();
  SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Finding allowed syscalls\n");
  findAllowedSysCalls();
  SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Finding used globals\n");
  findUsedGlobalVariables();
  findPrivateData();
  buildEntryPointsToIdxMap();
}

void Sandbox::reinit() {
  // clear everything
  callgates.clear();
  functionsVec.clear();
  functionsSet.clear();
  tlCallInsts.clear();
  callInsts.clear();
  // refFunctions.clear();
  creationPoints.clear();
  sysCallLimitPoints.clear();
  sysCallLimitPointToAllowedSysCalls.clear();
  sharedVarToPerms.clear();
  caps.clear();
  annotatedInputs.clear();
  objectInputTypes.clear();
  privateData.clear();
  entryPointToIdxMap.clear();

  init();
}

FunctionSet& Sandbox::getEntryPoints() {
  return entryPoints;
}

FunctionSet Sandbox::getInternalFunctions() {
  return internalFuncs;
}

ValueSet Sandbox::getInternalGlobals() {
  return internalGlobs;
}

map<Function*, unsigned>& Sandbox::getEntryPointToIdxMap() {
  return entryPointToIdxMap;
}

unsigned Sandbox::getIDForEntryPoint(Function* entry) {
  return entryPointToIdxMap[entry];
}

InputAnnotationMap Sandbox::getAnnotatedInputs() {
  return annotatedInputs;
}

StructVector Sandbox::getObjectInputTypes() {
  return objectInputTypes;
}

Function* Sandbox::getEnclosingFunc() {
  if (entryPoints.empty()) {
    Instruction* firstI = region.front();
    return firstI->getParent()->getParent();
  }
  return NULL;
} 

bool Sandbox::isRegionWithin(Function* F) {
  if (entryPoints.empty()) {
    if (!region.empty()) {
      Instruction* firstI = region.front();
      Function* enclosingFunc = firstI->getParent()->getParent();
      return enclosingFunc == F;
    }
  }
  return false;
}

string Sandbox::getName() {
  return name;
}

int Sandbox::getNameIdx() {
  return nameIdx;
}

FunctionVector Sandbox::getFunctions() {
  return functionsVec;
}

// FunctionVector Sandbox::getReferencedFunctions() {
//   return refRunctions;
// }

CallInstVector Sandbox::getTopLevelCalls() {
  return tlCallInsts;
}

CallInstVector Sandbox::getCalls() {
  return callInsts;
}

GlobalVariableIntMap Sandbox::getGlobalVarPerms() {
  return sharedVarToPerms;
}

ValueSet Sandbox::getLoadedGlobalVars() {
  return loadedGlobals;
}

ValueSet Sandbox::getStoredGlobalVars() {
  return storedGlobals;
}

ValueFunctionSetMap Sandbox::getCapabilities() {
  return caps;
}

map<const Value*, StringSet> Sandbox::getCapStrings() {
  return capsStrings;
}

bool Sandbox::isAllowedToReadGlobalVar(GlobalVariable* gv) {
  return sharedVarToPerms.find(gv) != sharedVarToPerms.end() && sharedVarToPerms[gv] & VAR_READ_MASK;
}

FunctionVector Sandbox::getCallgates() {
  return callgates;
}

bool Sandbox::isCallgate(Function* F) {
  return find(callgates.begin(), callgates.end(), F) != callgates.end();
}

int Sandbox::getClearances() {
  return clearances;
}

int Sandbox::getOverhead() {
  return overhead;
}

bool Sandbox::isPersistent() {
  return persistent;
}

CallInstVector Sandbox::getCreationPoints() {
  return creationPoints;
}

CallInstVector Sandbox::getSysCallLimitPoints() {
  return sysCallLimitPoints;
}

FunctionSet Sandbox::getAllowedSysCalls(CallInst* sysCallLimitPoint) {
  return sysCallLimitPointToAllowedSysCalls[sysCallLimitPoint];
}

bool Sandbox::containsFunction(Function* F) {
  return functionsSet.find(F) != functionsSet.end();
}

bool Sandbox::containsInstruction(Instruction* I) {
  if (entryPoints.empty()) {
    // This is a sandboxed region; first search the top-level
    // instructions in it.
    if (find(region.begin(), region.end(), I) != region.end()) {
      return true;
    }
  }
  Function* F = I->getParent()->getParent();
  SDEBUG("soaap.util.sandbox", 4, dbgs() << "Looking for " << F->getName() << " in " << name << ": " << containsFunction(F) << "\n");
  return containsFunction(F);
}

bool Sandbox::hasCallgate(Function* F) {
  return find(callgates.begin(), callgates.end(), F) != callgates.end();
}

void Sandbox::findSandboxedFunctions() {
  FunctionSet initialFuncs;
  if (entryPoints.empty()) {
    // scan the code region for the set of top-level functions being called
    for (Instruction* I : region) {
      if (CallInst* C  = dyn_cast<CallInst>(I)) {
        for (Function* F : CallGraphUtils::getCallees(C, this, module)) {
          if (F->isDeclaration()) continue;
          initialFuncs.insert(F);
        }
      }
    }
  }
  else {
    for (Function* F : entryPoints) {
      initialFuncs.insert(F);
    }
    for (Function* F : internalFuncs) {
      initialFuncs.insert(F);
    }
  }
  findSandboxedFunctionsHelper(initialFuncs);
  SDEBUG("soaap.util.sandbox", 3, dbgs() << "Number of sandboxed functions found: " << functionsVec.size() << ", " << functionsSet.size() << "\n");
}

void Sandbox::findSandboxedFunctionsHelper(FunctionSet fringe) {

  if (fringe.empty()) {
    return;
  }
   
  for (Function* F : fringe) {
    functionsVec.push_back(F);
    functionsSet.insert(F);
  }

  FunctionSet newFringe;
  for (Function* F : fringe) {
    SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Visiting " << F->getName() << "\n");
    SDEBUG("soaap.util.sandbox", 4, dbgs() << "Recursing on successors\n");
    for (Function* SuccFunc : CallGraphUtils::getCallees(F, this, module)) {
      SDEBUG("soaap.util.sandbox", 4, dbgs() << "succ: " << SuccFunc->getName() << "\n");
      // check if entry point to another sandbox
    
      if (SandboxUtils::isSandboxEntryPoint(module, SuccFunc) && !isEntryPoint(SuccFunc)) {
        continue;
      }
      else if (functionsSet.count(SuccFunc) == 0) {
        newFringe.insert(SuccFunc);
      }
    }

    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (StoreInst* SI = dyn_cast<StoreInst>(&*I)) {
        Value* operand = SI->getValueOperand()->stripPointerCasts();
        if (Function* func = dyn_cast<Function>(operand)) {
          // outs() << "Found a store function operand: " << func->getName() << "\n";
          newFringe.insert(func);
        }
      }
    }
  }

  findSandboxedFunctionsHelper(newFringe);
}

void Sandbox::findSandboxedCalls() {
  for (Function* F : functionsVec) {
    for (inst_iterator I=inst_begin(F), E=inst_end(F); I!=E; I++) {
      if (CallInst* C = dyn_cast<CallInst>(&*I)) {
        callInsts.push_back(C);
      }
    }
  }
  // if this sandbox doesn't have an entrypoint then search region instructions also
  // At the same time also record top-level call insts
  if (entryPoints.empty()) {
    for (Instruction* I : region) {
      if (CallInst* C = dyn_cast<CallInst>(I)) {
        callInsts.push_back(C);
        tlCallInsts.push_back(C);
      }
    }
  }
  else {
    for (Function* entryPoint : entryPoints) {
      for (inst_iterator I=inst_begin(entryPoint), E=inst_end(entryPoint); I!=E; I++) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          tlCallInsts.push_back(C);
        }
      }
    }
  }
}

/*
 * Find global variables that are annotated as being shared with this 
 * sandbox and record how they are allowed to be accessed
 */
void Sandbox::findSharedGlobalVariables() {

  /*
   * Global variable annotations are added to the global intrinsic
   * array called llvm.global.annotations:
   *
   * int fd __attribute__((annotate("var_read")));
   *
   * @fd = common global i32 0, align 4
   * @.str = private unnamed_addr constant [9 x i8] c"var_read\00", section "llvm.metadata"
   * @.str1 = private unnamed_addr constant [7 x i8] c"test-var.c\00", section "llvm.metadata"
   * @llvm.global.annotations = appending global [1 x { i8*, i8*, i8*, i32 }]
   *    [{ i8*, i8*, i8*, i32 }
   *     { i8* bitcast (i32* @fd to i8*),   // annotated global variable
   *       i8* getelementptr inbounds ([9 x i8]* @.str, i32 0, i32 0),  // annotation
   *       i8* getelementptr inbounds ([7 x i8]* @.str1, i32 0, i32 0), // file name
   *       i32 1 }] // line number
   */
  GlobalVariable* lga = module.getNamedGlobal("llvm.global.annotations");
  if (lga != NULL) {
    ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
    for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
      ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());

      // get the annotation value first
      GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
      ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
      StringRef annotationStrArrayCString = annotationStrArray->getAsCString();

      GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
      if (isa<GlobalVariable>(annotatedVal)) {
        GlobalVariable* annotatedVar = dyn_cast<GlobalVariable>(annotatedVal);
        if (annotationStrArrayCString.startswith(VAR_READ)) {
          StringRef sandboxName = annotationStrArrayCString.substr(strlen(VAR_READ)+1);
          if (sandboxName == name) {
            sharedVarToPerms[annotatedVar] |= VAR_READ_MASK;
          }
          SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Found annotated global var " << annotatedVar->getName() << "\n");
        }
        else if (annotationStrArrayCString.startswith(VAR_WRITE)) {
          StringRef sandboxName = annotationStrArrayCString.substr(strlen(VAR_WRITE)+1);
          if (sandboxName == name) {
            sharedVarToPerms[annotatedVar] |= VAR_WRITE_MASK;
          }
          SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Found annotated global var " << annotatedVar->getName() << "\n");
        }
      }
    }
  }
}

void Sandbox::findUsedGlobalVariables() {
  for (Function* F : getFunctions()) {
    for (BasicBlock& BB : F->getBasicBlockList()) {
      for (Instruction& I : BB.getInstList()) {
        if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
          Value* operand = load->getPointerOperand()->stripPointerCasts();
          if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(operand)) {
            operand = gep->getPointerOperand();
          }
          if (GlobalVariable* gv = dyn_cast<GlobalVariable>(operand)) {
            loadedGlobals.insert(gv);
          }
        } else if (StoreInst* store = dyn_cast<StoreInst>(&I)) {
          Value* operand = store->getPointerOperand()->stripPointerCasts();
          if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(operand)) {
            operand = gep->getPointerOperand();
          }
          if (GlobalVariable* gv = dyn_cast<GlobalVariable>(operand)) {
            /* TODO the analysis is "not concerned with externs */
            storedGlobals.insert(gv);
          }
        }
      }
    }
  }
}

/*
 * Find those functions that have been declared as being callgates
 * using the __callgates(...) macro. A callgate is a method whose
 * execution requires privileges different to those possessed by
 * the current sandbox.
 */
void Sandbox::findCallgates() {
  /*
   * Callgates are declared using the variadic macro
   * __callgates(fns...), that passes the functions as arguments
   * to the function __soaap_declare_callgates_helper:
   *
   * #define __soaap_callgates(fns...) \
   *    void __soaap_declare_callgates() { \
   *      __soaap_declare_callgates_helper(0, fns); \
   *    }
   *
   * Hence, we must find the "call @__soaap_declare_callgates_helper"
   * instruction and obtain the list of functions from its arguments
   */
  for (Function& F : module.getFunctionList()) {
    if (F.getName().startswith("__soaap_declare_callgates_helper_")) {
      SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Found __soaap_declare_callgates_helper_\n");
      StringRef sandboxName = F.getName().substr(strlen("__soaap_declare_callgates_helper")+1);
      SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Sandbox name: " << sandboxName << "\n");
      if (sandboxName == name) {
        for (User* U : F.users()) {
          if (CallInst* annotateCallgatesCall = dyn_cast<CallInst>(U)) {
            /*
             * Start at 1 because we skip the first unused argument
             * (The C language requires that there be at least one
             * non-variable argument).
             */
            for (unsigned int i=1; i<annotateCallgatesCall->getNumArgOperands(); i++) {
              Function* callgate = dyn_cast<Function>(annotateCallgatesCall->getArgOperand(i)->stripPointerCasts());
              SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Callgate " << i << " is " << callgate->getName() << "\n");
              callgates.push_back(callgate);
            }
          }
        }
      }
    }
  }
}

void Sandbox::findCapabilities() {
  /*
   * Find those file descriptor parameters that are shared with the
   * sandboxed method.
   *
   * These will be annotated parameters that are turned by
   * Clang/LLVM into calls to the intrinsic function
   * llvm.var.annotation, with the param as the arg. This is how
   * local variable annotations are represented in general in LLVM
   *
   * A parameter annotation looks like this:
   * void m(int ifd __soaap_fd(read, write)) { ... }
   *
   * It is turned into an intrinsic call as follows:
   *
   * call void @llvm.var.annotation(
   *   i8* %ifd.addr1,      // param (llvm creates a local var for the param by appending .addrN to the end of the param name)
   *   i8* getelementptr inbounds ([8 x i8]* @.str2, i32 0, i32 0),  // annotation
   *   i8* getelementptr inbounds ([30 x i8]* @.str3, i32 0, i32 0),  // file name
   *   i32 5)              // line number
   *
   * @.str2 = private unnamed_addr constant [8 x i8] c"fd_read, write\00", section "llvm.metadata"
   * @.str3 = private unnamed_addr constant [30 x i8] c"../../tests/test-param-decl.c\00", section "llvm.metadata"
   */
   
  for (Function* entryPoint : entryPoints) {
    for (BasicBlock& BB : entryPoint->getBasicBlockList()) {
      for (Instruction& I : BB.getInstList()) {
        if (CallInst* call = dyn_cast<CallInst>(&I)) {
          if (IntrinsicInst* II = dyn_cast<IntrinsicInst>(call)) {
            switch (II->getIntrinsicID()) {
              case Intrinsic::var_annotation: {
                Value* annotatedVar = dyn_cast<Value>(call->getOperand(0)->stripPointerCasts());

                GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(call->getOperand(1)->stripPointerCasts());
                ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
                StringRef annotationStrValStr = annotationStrValArray->getAsCString();

                SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Annotation: " << annotationStrValStr << "\n");

                /*
                 * Find out the enclosing function and record which
                 * param was annotated. We have to do this because
                 * llvm creates a local var for the param by appending
                 * '.addr1' and associates the annotation with the newly
                 * created local var i.e. see ifd and ifd.addr1 above
                 */
                SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Looking for AllocaDbgDeclare\n");
                if (DbgDeclareInst* dbgDecl = FindAllocaDbgDeclare(annotatedVar)) {
                  DILocalVariable* varDbg  = dbgDecl->getVariable();
                  string annotatedVarName = varDbg->getName().str();
                  SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Found DbgDeclareInst, annotated var: " << annotatedVarName << "\n");

                  // find the annotated parameter
                  Argument* annotatedArg = NULL;
                  for (Argument& arg : entryPoint->args()) {
                    if (arg.getName().str() == annotatedVarName) {
                      annotatedArg = &arg;
                    }
                  }

                  if (annotatedArg != NULL) {
                    if (annotationStrValStr.startswith(SOAAP_FD)) {
                      FunctionSet sysCalls;
                      StringSet sysCallsNames;
                      int subStrStartIdx = strlen(SOAAP_FD) + 1; //+1 because of _
                      string sysCallListCsv = annotationStrValStr.substr(subStrStartIdx);
                      SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_1 << " " << annotationStrValStr << " found: " << *annotatedVar << ", sysCallList: " << sysCallListCsv << "\n");
                      istringstream ss(sysCallListCsv);
                      string sysCallName;
                      while(getline(ss, sysCallName, ',')) {
                        // trim leading and trailing spaces
                        size_t start = sysCallName.find_first_not_of(" ");
                        size_t end = sysCallName.find_last_not_of(" ");
                        sysCallName = sysCallName.substr(start, end-start+1);
                        SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Syscall: " << sysCallName << "\n");
                        if (sysCallName == SOAAP_NO_SYSCALLS_ALLOWED) {
                          // Defensive: ideally no other system calls should have been listed
                          // but we play it safe and remove any that may have been annotated 
                          sysCalls.clear();
                          break;
                        }
                        if (Function* sysCallFn = module.getFunction(sysCallName)) {
                          SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Adding " << sysCallFn->getName() << "\n");
                          sysCalls.insert(sysCallFn);
                        }
                        sysCallsNames.insert(sysCallName);
                      }
                      caps[annotatedArg] = sysCalls;
                      capsStrings[annotatedArg] = sysCallsNames;
                    }
                    SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Found annotated file descriptor " << annotatedArg->getName() << "\n");
                  }
                }
                break;
              }
              case Intrinsic::ptr_annotation: {
                // annotation on struct member
                Value* annotatedVar = dyn_cast<Value>(II->getOperand(0)->stripPointerCasts());

                GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(II->getOperand(1)->stripPointerCasts());
                ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
                StringRef annotationStrValStr = annotationStrValArray->getAsCString();
                
                if (annotationStrValStr.startswith(SOAAP_FD)) {
                  int subStrStartIdx = strlen(SOAAP_FD) + 1; //+1 because of _
                  string sandboxNameAndSysCallListCsv = annotationStrValStr.substr(subStrStartIdx);
                  size_t quotePos = sandboxNameAndSysCallListCsv.find("\"");
                  if (quotePos != string::npos) {
                    size_t quote2Pos = sandboxNameAndSysCallListCsv.find("\"", quotePos+1);
                    if (quote2Pos != string::npos) {
                      string sandboxName = sandboxNameAndSysCallListCsv.substr(quotePos+1, quote2Pos-(quotePos+1));
                      if (sandboxName == name) {
                        outs() << "sandbox name: \"" << sandboxName << "\"\n";
                        string sysCallListCsv = sandboxNameAndSysCallListCsv.substr(quote2Pos+1);
                        outs() << "sysCallList: " << sysCallListCsv << "\n";
                        FunctionSet sysCalls;
                        SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_1 << " " << annotationStrValStr << " found: " << *annotatedVar << ", sysCallList: " << sysCallListCsv << "\n");
                        istringstream ss(sysCallListCsv);
                        string sysCallName;
                        while(getline(ss, sysCallName, ',')) {
                          outs() << "syscall: " << sysCallName << "\n";
                          // trim leading and trailing spaces
                          size_t start = sysCallName.find_first_not_of(" ");
                          size_t end = sysCallName.find_last_not_of(" ");
                          if (start != string::npos && end != string::npos) {
                            sysCallName = sysCallName.substr(start, end-start+1);
                            SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Syscall: " << sysCallName << "\n");
                            if (Function* sysCallFn = module.getFunction(sysCallName)) {
                              SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Adding " << sysCallFn->getName() << "\n");
                              sysCalls.insert(sysCallFn);
                            }
                          }
                        }
                        caps[II] = sysCalls;
                      }
                    }
                  }
                }
                break;
              }
              default: { }
            }
          }
        }
      }
    }
  }
}

void Sandbox::findCreationPoints() {
  // look for calls to __builtin_annotation(SOAAP_PERSISTENT_SANDBOX_CREATE_<NAME_OF_SANDBOX>)
  if (Function* AnnotFunc = module.getFunction("llvm.annotation.i32")) {
    for (User* U : AnnotFunc->users()) {
      if (IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U)) {
        GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
        ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
        StringRef annotationStrValCString = annotationStrValArray->getAsCString();
        
        if (annotationStrValCString.startswith(SOAAP_PERSISTENT_SANDBOX_CREATE)) {
          StringRef sandboxName = annotationStrValCString.substr(strlen(SOAAP_PERSISTENT_SANDBOX_CREATE)+1); //+1 because of _
          if (sandboxName == name) {
            SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Found persistent creation point: "; annotateCall->dump(););
            creationPoints.push_back(annotateCall);
            persistent = true;
          }
        }
        else if (annotationStrValCString.startswith(SOAAP_EPHEMERAL_SANDBOX_CREATE)) {
          StringRef sandboxName = annotationStrValCString.substr(strlen(SOAAP_EPHEMERAL_SANDBOX_CREATE)+1); //+1 because of _
          if (sandboxName == name) {
            SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Found ephemeral creation point: "; annotateCall->dump(););
            creationPoints.push_back(annotateCall);
            persistent = false;
          }
        }
      }
    }
  }
}

void Sandbox::findAllowedSysCalls() {
  // look for calls to __builtin_annotation(SOAAP_SYSCALLS_<LIST OF ALLOWED SYSCALLS>)
  if (Function* AnnotFunc = module.getFunction("llvm.annotation.i32")) {
    for (User* U : AnnotFunc->users()) {
      if (IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U)) {
        GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
        ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
        StringRef annotationStrValCString = annotationStrValArray->getAsCString();
        
        if (annotationStrValCString.startswith(SOAAP_SYSCALLS)) {
          // is this annotate call within this sandbox?
          bool inThisSandbox = false;
          if (entryPoints.empty()) {
            // first scan the code region for annotateCall
            for (Instruction* I : region) {
              if (I == annotateCall) {
                inThisSandbox = true;
                break;
              }
            }
          }
          if (!inThisSandbox) {
            // check called functions
            Function* enclosingFunc = annotateCall->getParent()->getParent();
            inThisSandbox = functionsSet.count(enclosingFunc) > 0;
          }

          if (inThisSandbox) {
            sysCallLimitPoints.push_back(annotateCall);
            FunctionSet allowedSysCalls;
            StringRef sysCallsListCsv = annotationStrValCString.substr(strlen(SOAAP_SYSCALLS)+1); //+1 because of _
            istringstream ss(sysCallsListCsv);
            string sysCall;
            while(getline(ss, sysCall, ',')) {
              // trim leading and trailing spaces
              size_t start = sysCall.find_first_not_of(" ");
              size_t end = sysCall.find_last_not_of(" ");
              sysCall = sysCall.substr(start, end-start+1);
              SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "SysCall: " << sysCall << "\n");
              if (sysCall == SOAAP_NO_SYSCALLS_ALLOWED) {
                // Defensive: ideally no other system calls should have been listed
                // but we play it safe and remove any that may have been annotated 
                allowedSysCalls.clear();
                break;
              }
              if (Function* sysCallFn = module.getFunction(sysCall)) {
                SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Adding Function* " << sysCallFn->getName() << "\n");
                allowedSysCalls.insert(sysCallFn);
              }
              else {
                SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Module doesn't call this syscall, so ignoring\n")
              }
            }
            sysCallLimitPointToAllowedSysCalls[annotateCall] = allowedSysCalls;
          }
        }
      }
    }
  }
}


// check that all entrypoint calls are dominated by a creation call
void Sandbox::validateCreationPoints() {
  // We traverse the whole-program CFG starting from main, treating sandbox 
  // creation-point calls as leaves (i.e. we don't traverse past them). If
  // after this, an entrypoint call is still reached, then a path has been
  // found without going through a creation-point call.
  if (Function* MainFunc = module.getFunction("main")) {
    BasicBlock& EntryBB = MainFunc->getEntryBlock();
    BasicBlockVector visited;
    InstTrace trace;
    validateCreationPointsHelper(&EntryBB, visited, trace);
  }
}

bool Sandbox::validateCreationPointsHelper(BasicBlock* BB, BasicBlockVector& visited, InstTrace& trace) {
  if (find(visited.begin(), visited.end(), BB) != visited.end()) {
    return false;
  }
  else {
    // Check instructions in BB (in order):
    //  - if a creation-point call, then stop.
    //  - if an entrypoint call, then flag an error.
    //  - if a function call, recurse on callee's entry bb
    // Otherwise, recurse on BB's successors
    visited.push_back(BB);
    for (Instruction& I : *BB) {
      if (find(creationPoints.begin(), creationPoints.end(), &I) != creationPoints.end()) {
        return true;
      }
      else if (CallInst* CI = dyn_cast<CallInst>(&I)) {
        trace.push_front(CI);
        FunctionSet callees = CallGraphUtils::getCallees(CI, ContextUtils::PRIV_CONTEXT, module);
        for (Function* callee : callees) {
          if (callee->isDeclaration()) continue;
          if (isEntryPoint(callee)) {
            // error
            outs() << "\n";
            outs() << " *** Found call to sandbox entrypoint \"" << callee->getName() << "\" that is not preceded by sandbox creation\n";
            outs() << " Possible trace:\n";
            PrettyPrinters::ppTrace(trace);
            outs() << "\n";
            trace.pop_front();
            return false;
          }
          else if (!callee->isDeclaration()) {
            // recurse on callee's entry bb
            if (validateCreationPointsHelper(&callee->getEntryBlock(), visited, trace)) {
              trace.pop_front();
              return true; // all paths through callee have a creation-point
            }
          }
        }
        trace.pop_front();
      }
    }

    // Recurse on BB's successors
    bool creationOnAllPaths = (succ_begin(BB) != succ_end(BB)); // true <-> at least one successor bb
    for (succ_iterator SI = succ_begin(BB), SE = succ_end(BB); SI != SE; ++SI) {
      BasicBlock* SuccBB = *SI;
      creationOnAllPaths = creationOnAllPaths && validateCreationPointsHelper(SuccBB, visited, trace);
    }

    return creationOnAllPaths;
  }
}

void Sandbox::findPrivateData() {
  
  // initialise with pointers to annotated fields and uses of annotated global variables
  if (Function* F = module.getFunction("llvm.ptr.annotation.p0i8")) {
    for (User* U : F->users()) {
      if (IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U)) {
        Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());

        GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
        ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
        StringRef annotationStrValCString = annotationStrValArray->getAsCString();
        
        if (annotationStrValCString.startswith(SANDBOX_PRIVATE)) {
          StringRef sandboxName = annotationStrValCString.substr(strlen(SANDBOX_PRIVATE)+1); //+1 because of _
          if (sandboxName == name) {
            SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_1 
                                                   << "Found private data annotation for "
                                                   << "sandbox \"" << name << "\" for variable "
                                                   << "\"" << annotatedVar->getName() << "\"");
            privateData.insert(annotateCall);
          }
        }
        
        SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_1 << "Sandbox-private struct field: "; annotatedVar->dump(););
      }
    }
  }
  
  if (Function* F = module.getFunction("llvm.var.annotation")) {
    for (User* U : F->users()) {
      if (IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U)) {
        Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());

        GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
        ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
        StringRef annotationStrValCString = annotationStrValArray->getAsCString();
        if (annotationStrValCString.startswith(SANDBOX_PRIVATE)) {
          StringRef sandboxName = annotationStrValCString.substr(strlen(SANDBOX_PRIVATE)+1); //+1 because of _
          if (sandboxName == name) {
            privateData.insert(annotateCall);
          }
        
          SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_1 << "Sandbox-private local variable: "; annotatedVar->dump(););
        }
      }
    }
  }

  // annotations on variables are stored in the llvm.global.annotations global
  // array
  if (GlobalVariable* lga = module.getNamedGlobal("llvm.global.annotations")) {
    ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
    for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
      ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());

      // get the annotation value first
      GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
      ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
      StringRef annotationStrArrayCString = annotationStrArray->getAsCString();
      if (annotationStrArrayCString.startswith(SANDBOX_PRIVATE)) {
        StringRef sandboxName = annotationStrArrayCString.substr(strlen(SANDBOX_PRIVATE)+1);
        if (sandboxName == name) {
          GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
          if (isa<GlobalVariable>(annotatedVal)) {
            privateData.insert(annotatedVal);

            SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_1 << "Found sandbox-private global variable \"" << annotatedVal->getName() << "\"\n";)
          }
        }
      }
    }
  }
}

void Sandbox::findAnnotatedInputs() {
  Function* FVA = module.getFunction("llvm.var.annotation");
  if (!FVA) return;
  /* TODO handle the annotated region case. */
  if (entryPoints.empty()) return;
  for (User* U : FVA->users()) {
    IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U);
    if (!annotateCall) continue;

    Function* F = annotateCall->getParent()->getParent();
    std::string funcName = F->getName().str();
    if (!entryPoints.count(F)) continue;

    Value* annotatedVar = dyn_cast<Value>(
        annotateCall->getOperand(0)->stripPointerCasts());

    /* Get the annotation as string. */
    GlobalVariable* annotationStr = dyn_cast<GlobalVariable>(
        annotateCall->getOperand(1)->stripPointerCasts());
    ConstantDataArray* annotationStrArr = dyn_cast<ConstantDataArray>(
        annotationStr->getInitializer());
    StringRef annotationCString = annotationStrArr->getAsCString();

    DbgDeclareInst* dbgDecl = FindAllocaDbgDeclare(annotatedVar);
    if (!dbgDecl) continue;

    DILocalVariable* varDbg = dbgDecl->getVariable();
    std::string annotatedArgName = varDbg->getName().str();
    std::string linkedArgName = "";
    Value* annotatedArg = NULL;
    Value* linkedArg = NULL;

    InputAnnotation annotationData;
    SmallVector<StringRef, 3> annotationParts;
    annotationCString.split(annotationParts, "_");

    // if (annotationParts.size() < 2) {
    //   // error
    //   outs() << "\n";
    //   outs() << " *** Found an unsupported input annotation \"" <<
    //     annotationCString.str() << "\" for argument \"" << annotatedArgName <<
    //     "\" in function \"" << funcName << "\n";
    //   exit(1);
    // }

    bool invalid = false;

    if (annotationParts[0] == ACCESS_IN) {
      annotationData.access = InputAccess::In;
    } else if (annotationParts[0] == ACCESS_INOUT) {
      annotationData.access = InputAccess::Inout;
    } else if (annotationParts[0] == ACCESS_OUT) {
      annotationData.access = InputAccess::Out;
    } else if (annotationParts[0] == HANDLE) {
      annotationData.access = InputAccess::Inout;
      annotationData.type = InputType::Handle;
    } else {
      invalid = true;
    }

    if (annotationParts.size() > 1) {
      if (annotationParts[1] == TYPE_PTR) {
        annotationData.type = InputType::Pointer;
      } else if (annotationParts[1] == TYPE_Z) {
        annotationData.type = InputType::NullTerminatedBuffer;
      } else if (annotationParts[1] == TYPE_BUFF) {
        annotationData.type = InputType::Buffer;
      } else {
        invalid = true;
      }
    }

    if (annotationParts.size() > 2) {
      if (annotationData.type == InputType::Buffer) {
        if (annotationParts[2] == OPT) {
          annotationData.optional = true;
          linkedArgName = annotationParts[3];
        } else {
          linkedArgName = annotationParts[2];
        }
      } else if (annotationParts[2] == OPT) {
        annotationData.optional = true;
      }
    }

    if (annotationParts.size() > 2 && annotationParts[2] == OPT) {
      annotationData.optional = true;
    }

    if (invalid) {
      if (annotationCString.startswith(SOAAP_FD)) {
        annotationData.access = InputAccess::Copy;
        annotationData.type = InputType::FileDescriptor;
      } else {
        // error
        outs() << "\n";
        outs() << " *** Found an unsupported input annotation \"" <<
          annotationCString.str() << "\" for argument \"" << annotatedArgName <<
          "\" in function \"" << funcName << "\n";
        exit(1);
      }
    }

    for (auto A = F->arg_begin(); A != F->arg_end(); A++) {
      std::string argName = A->getName().str();
      if (argName == annotatedArgName) {
        annotatedArg = dyn_cast<Value>(A);
      } else if (argName == linkedArgName) {
        linkedArg = dyn_cast<Value>(A);
      }
    }

    if (annotatedArg == NULL) {
      // error
      outs() << "\n";
      outs() << " *** Function \"" << funcName << "\" has no argument named \"" <<
        annotatedArgName << "\n";
      return;
    }

    if (annotationData.type == InputType::Buffer ||
        annotationData.type == InputType::ByteBuffer) {
      if (linkedArg == NULL) {
        // error
        outs() << "\n";
        outs() << " *** Invalid annotation \"" << annotationCString.str() <<
          "\": Function \"" << funcName << "\" has no argument named \"" <<
          linkedArgName << "\n";
        return;
      }

      annotationData.linkedArg = linkedArg;
    }

    annotatedInputs[annotatedArg] = annotationData;
  }
}

void Sandbox::findObjectInputTypes() {
  for (const auto& i : annotatedInputs) {
    Type* valTy = i.first->getType();
    if (!valTy->isPointerTy()) continue;
    Type* elemTy = valTy->getPointerElementType();
    if (!isa<StructType>(elemTy)) continue;
    /* TODO check if the struct is a class. */
    objectInputTypes.push_back(dyn_cast<StructType>(elemTy));
  }
}

ValueSet Sandbox::getPrivateData() {
  return privateData;
}

InstVector Sandbox::getRegion() {
  return region;
}

bool Sandbox::isEntryPoint(Function* F) {
  return entryPoints.count(F) == 1;
}

void Sandbox::buildEntryPointsToIdxMap() {
  unsigned idx = 0;
  for (Function* F : entryPoints) {
    entryPointToIdxMap[F] = idx;
    ++idx;
  }
}
