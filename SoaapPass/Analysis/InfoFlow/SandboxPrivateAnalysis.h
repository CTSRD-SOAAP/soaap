#ifndef SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {

  class SandboxPrivateAnalysis : public InfoFlowAnalysis {
    public:
      SandboxPrivateAnalysis(FunctionVector& privMethods, FunctionVector& sboxMethods, 
                             FunctionVector& allMethods, FunctionVector& cgates, 
                             FunctionIntMap& sboxMethodToNames)
                             : privilegedMethods(privMethods), sandboxedMethods(sboxMethods), 
                               allReachableMethods(allMethods), callgates(cgates), 
                               sandboxedMethodToNames(sboxMethodToNames) { }
      virtual void initialise(ValueList& worklist, Module& M);
      virtual void postDataFlowAnalysis(Module& M);

    private:
      FunctionVector privilegedMethods;
      FunctionVector sandboxedMethods;
      FunctionVector allReachableMethods;
      FunctionVector callgates;
      FunctionIntMap sandboxedMethodToNames;

  };
}
#endif 
