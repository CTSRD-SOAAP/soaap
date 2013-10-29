#ifndef SOAAP_ANALYSIS_INFOFLOW_FPANNOTATEDTARGETSANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_FPANNOTATEDTARGETSANALYSIS_H

#include "Analysis/InfoFlow/FPTargetsAnalysis.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {
  class FPAnnotatedTargetsAnalysis: public FPTargetsAnalysis {
    protected:
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes);
  };
}

#endif 
