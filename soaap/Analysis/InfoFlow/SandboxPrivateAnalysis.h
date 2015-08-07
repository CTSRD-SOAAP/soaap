#ifndef SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H

#include "Analysis/InfoFlow/DeclassifierAnalysis.h"
#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {

  class SandboxPrivateAnalysis : public InfoFlowAnalysis<int> {
    public:
      SandboxPrivateAnalysis(bool contextInsensitive, FunctionSet& privMethods) : InfoFlowAnalysis<int>(contextInsensitive), privilegedMethods(privMethods) { }
    
    protected:
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual bool propagateToValue(const Value* from, const Value* to, Context* cFrom, Context* cTo, Module& M);
      virtual bool performMeet(int from, int& to);
      virtual bool performUnion(int from, int& to);
      virtual int bottomValue() { return int(); }
      virtual bool checkEqual(int f1, int f2);
      virtual string stringifyFact(int fact);

    private:
      FunctionSet privilegedMethods;
      DeclassifierAnalysis declassifierAnalysis;
      map<int, Value*> bitIdxToSource;
      map<int, int> bitIdxToPrivSandboxIdxs;
      map<Value*, IntrinsicInst*> varToAnnotateCall;

      int convertStateToBitIdxs(int& vs);
      void outputSources(Context* C, Value* V);
  };
}
#endif 
