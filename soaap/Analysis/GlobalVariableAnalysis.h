#ifndef SOAAP_ANALYSIS_GLOBALVARIABLEANALYSIS_H
#define SOAAP_ANALYSIS_GLOBALVARIABLEANALYSIS_H

#include "Analysis/CFGFlow/CFGFlowAnalysis.h"

namespace soaap {

  class GlobalVariableAnalysis : public CFGFlowAnalysis<int> {
    public:
      GlobalVariableAnalysis(FunctionSet& privMethods) : privilegedMethods(privMethods) { }
    
    protected:
      virtual void initialise(QueueSet<BasicBlock*>& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);

    private:
      FunctionSet privilegedMethods;
      string findGlobalDeclaration(Module& M, GlobalVariable* G);
  };

}

#endif
