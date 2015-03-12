#ifndef SOAAP_ANALYSIS_SANDBOXEDFUNCANALYSIS_H
#define SOAAP_ANALYSIS_SANDBOXEDFUNCANALYSIS_H

#include "Analysis/Analysis.h"
#include "Common/Typedefs.h"

namespace soaap {

  class SandboxedFuncAnalysis : public Analysis {
    public:
      virtual void doAnalysis(Module& M, SandboxVector& sandboxes);
    
    private:
      map<Function*, SandboxVector> funcToSandboxes;
  };

}

#endif
