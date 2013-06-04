#ifndef SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {

  class SandboxPrivateAnalysis : public InfoFlowAnalysis {
    public:
      SandboxPrivateAnalysis(FunctionVector& privMethods, FunctionVector& cgates) 
                             : privilegedMethods(privMethods), callgates(cgates) { } 
      virtual void initialise(ValueList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);

    private:
      FunctionVector privilegedMethods;
      FunctionVector callgates;
  };
}
#endif 
