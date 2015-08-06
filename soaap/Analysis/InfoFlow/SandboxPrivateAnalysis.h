#ifndef SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H

#include "Analysis/InfoFlow/DeclassifierAnalysis.h"
#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {

  class SandboxPrivateAnalysis : public InfoFlowAnalysis<ValueSet> {
    public:
      SandboxPrivateAnalysis(bool contextInsensitive, FunctionSet& privMethods) : InfoFlowAnalysis<ValueSet>(contextInsensitive), privilegedMethods(privMethods) { }
    
    protected:
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual bool propagateToValue(const Value* from, const Value* to, Context* cFrom, Context* cTo, Module& M);
      virtual bool performMeet(ValueSet from, ValueSet& to);
      virtual bool performUnion(ValueSet from, ValueSet& to);
      virtual ValueSet bottomValue() { return ValueSet(); }
      virtual bool checkEqual(ValueSet f1, ValueSet f2);
      virtual string stringifyFact(ValueSet fact);

    private:
      FunctionSet privilegedMethods;
      DeclassifierAnalysis declassifierAnalysis;
      map<Value*, int> valueToPrivSandboxes;
      map<Value*, IntrinsicInst*> varToAnnotateCall;

      int convertStateToBitIdxs(ValueSet& vs);
      void outputSources(Context* C, Value* V);
  };
}
#endif 
