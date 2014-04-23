#ifndef SOAAP_ANALYSIS_CFGFLOW_SYSCALLSANALYSIS_H
#define SOAAP_ANALYSIS_CFGFLOW_SYSCALLSANALYSIS_H

#include "Analysis/CFGFlow/CFGFlowAnalysis.h"
#include "OS/FreeBSDSysCallProvider.h"

#include "llvm/ADT/BitVector.h"

namespace soaap {

  class SysCallsAnalysis : public CFGFlowAnalysis<BitVector> {
    protected:
      virtual void initialise(QueueSet<BasicBlock*>& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual BitVector bottomValue() { return BitVector(); }
      virtual string stringifyFact(BitVector& fact);

    private:
      FreeBSDSysCallProvider freeBSDSysCallProvider;
  };

}

#endif