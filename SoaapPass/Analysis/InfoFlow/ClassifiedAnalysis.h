#ifndef SOAAP_ANALYSIS_INFOFLOW_CLASSIFIEDANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_CLASSIFIEDANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {

  class ClassifiedAnalysis: public InfoFlowAnalysis {
    public:
      ClassifiedAnalysis(FunctionVector& sboxMethods, 
                         FunctionIntMap& sboxMethodToClearances)
                         : sandboxedMethods(sboxMethods), 
                           sandboxedMethodToClearances(sboxMethodToClearances) { }
      virtual void initialise(ValueList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);

    private:
      FunctionVector sandboxedMethods;
      FunctionIntMap sandboxedMethodToClearances;
  };
}

#endif 
