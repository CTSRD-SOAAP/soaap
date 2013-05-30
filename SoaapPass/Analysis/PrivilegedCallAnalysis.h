#ifndef SOAAP_ANALYSIS_PRIVILEGEDCALLANALYSIS_H
#define SOAAP_ANALYSIS_PRIVILEGEDCALLANALYSIS_H

#include "Analysis/Analysis.h"

namespace soaap {

  class PrivilegedCallAnalysis : public Analysis {
    public:
      virtual void doAnalysis(Module& M, SandboxVector& sandboxes);
    
    private:
      
  };

}

#endif
