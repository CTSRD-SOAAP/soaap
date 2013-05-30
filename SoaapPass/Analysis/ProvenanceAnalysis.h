#ifndef SOAAP_ANALYSIS_PROVENANCEANALYSIS_H
#define SOAAP_ANALYSIS_PROVENANCEANALYSIS_H

#include "Analysis/Analysis.h"

namespace soaap {

  class ProvenanceAnalysis : public Analysis {
    public:
      virtual void doAnalysis(Module& M, SandboxVector& sandboxes);
    
    private:
      
  };

}

#endif
