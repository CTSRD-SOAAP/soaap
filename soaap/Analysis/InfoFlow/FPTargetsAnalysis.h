#ifndef SOAAP_ANALYSIS_INFOFLOW_FPTARGETSANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_FPTARGETSANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {
  class FPTargetsAnalysis: public InfoFlowAnalysis<FunctionSet> {
    public:
      FPTargetsAnalysis() : InfoFlowAnalysis<FunctionSet>(true) { }
      virtual FunctionSet getTargets(Value* FP);
      virtual bool hasTargets() { return !state.empty(); }

    protected:
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) = 0;
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual bool performMeet(FunctionSet from, FunctionSet& to);
      virtual FunctionSet bottomValue() { return FunctionSet(); }
      virtual string stringifyFact(FunctionSet fact);
      virtual void stateChangedForFunctionPointer(CallInst* CI, const Value* FP, FunctionSet& newState);
  };
}

#endif 
