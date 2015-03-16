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

    private:
      SandboxVector sandboxes;
      FunctionSet privilegedMethods;
      StringVector vulnerableVendors;
      shared_ptr<SandboxPlatform> sandboxPlatform;
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
      void buildRPCGraph(Module& M);
  };

}

#endif
