#ifndef SOAAP_ANALYSIS_INFOFLOW_FPINFERREDTARGETSANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_FPINFERREDTARGETSANALYSIS_H

#include "Analysis/InfoFlow/FPTargetsAnalysis.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {
  class FPInferredTargetsAnalysis: public FPTargetsAnalysis {
    public:
      FPInferredTargetsAnalysis(bool c) : FPTargetsAnalysis(c) { }

    protected:
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual void findAllFunctionPointersInValue(Value* V, ValueContextPairList& worklist, ValueSet& visited);

    private:
      void addInferredFunction(Function *, ContextVector, Value *,
                               ValueContextPairList& worklist);
  };
}

#endif 
