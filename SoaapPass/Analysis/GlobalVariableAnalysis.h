#ifndef SOAAP_ANALYSIS_GLOBALVARIABLEANALYSIS_H
#define SOAAP_ANALYSIS_GLOBALVARIABLEANALYSIS_H

#include "Analysis/Analysis.h"

namespace soaap {

  class GlobalVariableAnalysis : public Analysis {
    public:
      virtual void doAnalysis(Module& M);
    
    private:
      
  };

}

#endif
