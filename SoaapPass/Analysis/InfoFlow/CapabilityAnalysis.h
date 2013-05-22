#ifndef SOAAP_ANALYSIS_INFOFLOW_CAPABILITYANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_CAPABILITYANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"
#include <string>

using namespace llvm;
using namespace std;

namespace soaap {

  class CapabilityAnalysis : public InfoFlowAnalysis {
    public:
      CapabilityAnalysis(FunctionSet& sboxedMethods) : sandboxedMethods(sboxedMethods) { }
      virtual void initialise(ValueList& worklist, Module& M);
      virtual void postDataFlowAnalysis(Module& M);
      virtual int performMeet(int fromVal, int toVal);

    private:
      FunctionSet sandboxedMethods;
      void validateDescriptorAccesses(Module& M, string syscall, int requiredPerm);
  };
}

#endif 
