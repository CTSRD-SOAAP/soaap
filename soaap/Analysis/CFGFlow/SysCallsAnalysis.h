#ifndef SOAAP_ANALYSIS_CFGFLOW_SYSCALLSANALYSIS_H
#define SOAAP_ANALYSIS_CFGFLOW_SYSCALLSANALYSIS_H

#include "Analysis/CFGFlow/CFGFlowAnalysis.h"
#include "OS/FreeBSDSysCallProvider.h"
#include "OS/Sandbox/SandboxPlatform.h"

#include "llvm/ADT/BitVector.h"

namespace soaap {

  class SysCallsAnalysis : public CFGFlowAnalysis<BitVector> {
    public:
      SysCallsAnalysis(shared_ptr<SandboxPlatform>& platform) : sandboxPlatform(platform) { }
      bool allowedToPerformNamedSystemCallAtSandboxedPoint(Instruction* I, string sysCall);

    protected:
      virtual void initialise(QueueSet<BasicBlock*>& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual BitVector bottomValue() { return BitVector(); }
      virtual string stringifyFact(BitVector& fact);

    private:
      FreeBSDSysCallProvider freeBSDSysCallProvider;
      shared_ptr<SandboxPlatform> sandboxPlatform;
  };

}

#endif
