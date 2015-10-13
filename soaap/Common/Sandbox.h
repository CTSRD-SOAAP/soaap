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

#ifndef SOAAP_COMMON_SANDBOX_H
#define SOAAP_COMMON_SANDBOX_H

#include "Analysis/InfoFlow/Context.h"
#include "Common/Typedefs.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/CallGraph.h"

using namespace llvm;

namespace soaap {
  typedef map<GlobalVariable*,int> GlobalVariableIntMap;
  class Sandbox : public Context {
    public:
      Sandbox(string n, int i, FunctionSet entries, bool p, Module& m, int o, int c);
      Sandbox(string n, int i, InstVector& region, bool p, Module& m);
      string getName();
      int getNameIdx();
      FunctionSet getEntryPoints();
      Function* getEnclosingFunc();
      bool isRegionWithin(Function* F);
      FunctionVector getFunctions();
      CallInstVector getCalls();
      CallInstVector getTopLevelCalls();
      InstVector getRegion();
      GlobalVariableIntMap getGlobalVarPerms();
      ValueFunctionSetMap getCapabilities();
      bool isAllowedToReadGlobalVar(GlobalVariable* gv);
      FunctionVector getCallgates();
      bool isCallgate(Function* F);
      bool isEntryPoint(Function* F);
      int getClearances();
      int getOverhead();
      bool isPersistent();
      CallInstVector getCreationPoints();
      CallInstVector getSysCallLimitPoints();
      FunctionSet getAllowedSysCalls(CallInst* sysCallLimitPoint);
      ValueSet getPrivateData();
      bool containsFunction(Function* F);
      bool containsInstruction(Instruction* I);
      bool hasCallgate(Function* F);
      void validateCreationPoints();
      void reinit();
      static bool classof(const Context* C) { return C->getKind() == CK_SANDBOX; }

    private:
      Module& module;
      string name;
      int nameIdx;
      FunctionSet entryPoints;
      InstVector region;
      bool persistent;
      int clearances;
      FunctionVector callgates;
      FunctionVector functionsVec;
      DenseSet<Function*> functionsSet;
      CallInstVector tlCallInsts;
      CallInstVector callInsts;
      CallInstVector creationPoints;
      CallInstVector sysCallLimitPoints;
      map<CallInst*,FunctionSet> sysCallLimitPointToAllowedSysCalls;
      GlobalVariableIntMap sharedVarToPerms;
      ValueFunctionSetMap caps;
      int overhead;
      ValueSet privateData;
      
      void init();
      void findSandboxedFunctions();
      void findSandboxedFunctionsHelper(FunctionSet funcs);
      void findSandboxedCalls();
      void findSharedGlobalVariables();
      void findCallgates();
      void findCapabilities();
      void findAllowedSysCalls();
      void findCreationPoints();
      void findPrivateData();
      bool validateCreationPointsHelper(BasicBlock* BB, BasicBlockVector& visited, InstTrace& trace);
  };
  typedef SmallVector<Sandbox*,16> SandboxVector;
}

#endif
