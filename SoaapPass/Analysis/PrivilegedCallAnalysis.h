#ifndef SOAAP_ANALYSIS_PRIVILEGEDCALLANALYSIS_H
#define SOAAP_ANALYSIS_PRIVILEGEDCALLANALYSIS_H

#include "Analysis/Analysis.h"
#include "Common/Typedefs.h"

namespace soaap {

  class PrivilegedCallAnalysis : public Analysis {
    public:
      virtual void doAnalysis(Module& M, SandboxVector& sandboxes);
    
    private:
      FunctionVector privAnnotFuncs; // function annotated as being privileged
  };

}

#endif
