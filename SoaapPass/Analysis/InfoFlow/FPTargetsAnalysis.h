#ifndef SOAAP_ANALYSIS_INFOFLOW_FPTARGETSANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_FPTARGETSANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {
  class FPTargetsAnalysis: public InfoFlowAnalysis<FunctionVector> {
    public:
      virtual FunctionVector getTargets(Value* FP);

    protected:
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) = 0;
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual FunctionVector performMeet(FunctionVector from, FunctionVector to);
      bool prevIsContextInsensitiveAnalysis; // to cache prev value of 
                                           // ContextUtils::IsContextInsensitiveAnalysis
  };
}

#endif 
