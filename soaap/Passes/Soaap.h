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

#ifndef SOAAP_PASSES_SOAAP_H
#define SOAAP_PASSES_SOAAP_H

#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "soaap.h"

#include "Common/Typedefs.h"
#include "Common/Sandbox.h"
#include "Analysis/VulnerabilityAnalysis.h"
#include "Analysis/PrivilegedCallAnalysis.h"
#include "Analysis/CFGFlow/GlobalVariableAnalysis.h"
#include "Analysis/InfoFlow/AccessOriginAnalysis.h"
#include "Analysis/InfoFlow/SandboxPrivateAnalysis.h"
#include "Analysis/InfoFlow/ClassifiedAnalysis.h"
#include "Analysis/InfoFlow/CapabilityAnalysis.h"
#include "Instrument/PerformanceEmulationInstrumenter.h"
#include "OS/Sandbox/SandboxPlatform.h"
#include "Util/CallGraphUtils.h"
#include "Util/ClassHierarchyUtils.h"
#include "Util/ContextUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/SandboxUtils.h"

using namespace llvm;
using namespace std;

namespace llvm {
  void initializeSoaapPass(PassRegistry&);
}

namespace soaap {
  struct Soaap : public ModulePass {
    public:
      static char ID;
      Soaap() : ModulePass(ID) { }
      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnModule(Module& M);
      void generateSandboxingModules(std::vector<Module*>* modules);

    private:
      SandboxVector sandboxes;
      FunctionSet privilegedMethods;
      shared_ptr<SandboxPlatform> sandboxPlatform;
      shared_ptr<SysCallProvider> operatingSystem;
      std::vector<Module*>* genModules;
      void processCmdLineArgs(Module& M);
      void checkPrivilegedCalls(Module& M);
      void checkLeakedRights(Module& M);
      void checkOriginOfAccesses(Module& M);
      void findSandboxes(Module& M);
      void checkPropagationOfSandboxPrivateData(Module& M);
      void checkPropagationOfClassifiedData(Module& M);
      void checkFileDescriptors(Module& M);
      void checkSysCalls(Module& M);
      void calculatePrivilegedMethods(Module& M);
      void checkGlobalVariables(Module& M);
      void checkSandboxedFuncs(Module& M);
      void instrumentPerfEmul(Module& M);
      void generateSandboxes(Module& M);
      void buildRPCGraph(Module& M);
  };

}

#endif
