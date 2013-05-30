#ifndef SOAAP_ANALYSIS_GLOBALVARIABLEANALYSIS_H
#define SOAAP_ANALYSIS_GLOBALVARIABLEANALYSIS_H

#include "Analysis/Analysis.h"

namespace soaap {

  class GlobalVariableAnalysis : public Analysis {
    public:
      GlobalVariableAnalysis(FunctionVector& privMethods) : privilegedMethods(privMethods) { }
      virtual void doAnalysis(Module& M, SandboxVector& sandboxes);
    
    private:
      FunctionVector privilegedMethods;
  };

}

#endif
