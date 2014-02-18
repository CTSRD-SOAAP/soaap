#ifndef SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H

#include "Analysis/InfoFlow/DeclassifierAnalysis.h"
#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {

  class SandboxPrivateAnalysis : public InfoFlowAnalysis<int> {
    public:
      SandboxPrivateAnalysis(bool contextInsensitive, FunctionVector& privMethods) : InfoFlowAnalysis<int>(contextInsensitive), privilegedMethods(privMethods) { }
    
    protected:
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual bool propagateToValue(const Value* from, const Value* to, Context* cFrom, Context* cTo, Module& M);
      virtual int performMeet(int from, int to);
      virtual int bottomValue() { return 0; }
      virtual string stringifyFact(int fact);

    private:
      FunctionVector privilegedMethods;
      DeclassifierAnalysis declassifierAnalysis;
  };
}
#endif 
