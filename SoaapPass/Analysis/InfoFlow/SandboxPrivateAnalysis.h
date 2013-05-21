#ifndef SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {

  class SandboxPrivateAnalysis : public InfoFlowAnalysis {
    public:
      SandboxPrivateAnalysis(FunctionVector& privMethods, FunctionSet& sboxMethods, 
                             FunctionSet& allMethods, FunctionVector& cgates, 
                             FunctionIntMap& sboxMethodToNames)
                             : privilegedMethods(privMethods), sandboxedMethods(sboxMethods), 
                               allReachableMethods(allMethods), callgates(cgates), 
                               sandboxedMethodToNames(sboxMethodToNames) { }
      virtual void initialise(ValueList& worklist, Module& M);
      virtual void postDataFlowAnalysis(Module& M);

    private:
      FunctionVector privilegedMethods;
      FunctionSet sandboxedMethods;
      FunctionSet allReachableMethods;
      FunctionVector callgates;
      FunctionIntMap sandboxedMethodToNames;

  };
}
#endif 
