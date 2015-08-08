#ifndef SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_SANDBOXPRIVATEANALYSIS_H

#include "Analysis/InfoFlow/DeclassifierAnalysis.h"
#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {

  class SandboxPrivateAnalysis : public InfoFlowAnalysis<int> {
    public:
      SandboxPrivateAnalysis(bool contextInsensitive, FunctionSet& privMethods, SandboxVector& sboxes) : InfoFlowAnalysis<int>(contextInsensitive), privilegedMethods(privMethods), sandboxes(sboxes) { }
    
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
      SandboxVector sandboxes;
      DeclassifierAnalysis declassifierAnalysis;
      map<int, Instruction*> bitIdxToSource;
      map<int, int> bitIdxToPrivSandboxIdxs;
      map<Value*, IntrinsicInst*> varToAnnotateCall;
      map<Function*, map<Function*,InstTrace> > funcToShortestCallPaths;

      int convertStateToBitIdxs(int& vs);
      void outputSources(Context* C, Value* V, Function* F);
      //InstTrace findPrivilegedPathToFunction(Function* Target, int taint);
      //InstTrace findSandboxedPathToFunction(Function* Target, Sandbox* S, int taint);
      void calculateShortestCallPathsFromFunc(Function* F, Context* C, int taint);
      bool doesCallPropagateTaint(CallInst* C, int taint, Context* Ctx);
  };
}
#endif 
