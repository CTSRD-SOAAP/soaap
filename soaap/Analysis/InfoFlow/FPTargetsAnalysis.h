#ifndef SOAAP_ANALYSIS_INFOFLOW_FPTARGETSANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_FPTARGETSANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

#include "llvm/ADT/BitVector.h"

using namespace llvm;

namespace soaap {
  class FPTargetsAnalysis: public InfoFlowAnalysis<BitVector> {
    public:
      FPTargetsAnalysis(bool contextInsens) : InfoFlowAnalysis<BitVector>(contextInsens, false) { }
      virtual FunctionSet getTargets(Value* FP, Context* C);
      virtual bool hasTargets() { return !state.empty(); } // TODO: should we be looking inside state?

    protected:
      static map<Function*,int> funcToIdx;
      static map<int,Function*> idxToFunc;
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) = 0;
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual bool performMeet(BitVector from, BitVector& to);
      virtual bool performUnion(BitVector from, BitVector& to);
      virtual BitVector bottomValue() { return BitVector(); }
      virtual bool checkEqual(BitVector f1, BitVector f2) { return f1 == f2; }
      virtual string stringifyFact(BitVector fact);
      virtual void stateChangedForFunctionPointer(CallInst* CI, const Value* FP, Context* C, BitVector& newState);
      virtual FunctionSet convertBitVectorToFunctionSet(BitVector vector);
      virtual BitVector convertFunctionSetToBitVector(FunctionSet funcs);
      virtual void setBitVector(BitVector& vector, Function* F);
      virtual bool areTypeCompatible(FunctionType* FT1, FunctionType* FT2);
  };
}

#endif 
