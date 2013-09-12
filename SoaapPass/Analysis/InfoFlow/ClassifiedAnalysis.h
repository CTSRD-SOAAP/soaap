#ifndef SOAAP_ANALYSIS_INFOFLOW_CLASSIFIEDANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_CLASSIFIEDANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {

  class ClassifiedAnalysis: public InfoFlowAnalysis<int> {
    public:
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
  };
}

#endif 
