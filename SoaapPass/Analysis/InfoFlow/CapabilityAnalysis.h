#ifndef SOAAP_ANALYSIS_INFOFLOW_CAPABILITYANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_CAPABILITYANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"
#include <string>

using namespace llvm;
using namespace std;

namespace soaap {

  class CapabilityAnalysis : public InfoFlowAnalysis<int> {
    protected:
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual int performMeet(int fromVal, int toVal);
      virtual int bottomValue() { return 0; }

    private:
      void validateDescriptorAccesses(Module& M, SandboxVector& sandboxes, string syscall, int requiredPerm);
  };
}

#endif 
