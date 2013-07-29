#ifndef SOAAP_ANALYSIS_INFOFLOW_INFOFLOWANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_INFOFLOWANALYSIS_H

#include <map>
#include <list>

#include "llvm/IR/Value.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/ProfileInfo.h"
#include "llvm/Analysis/CallGraph.h"

#include "Analysis/Analysis.h"

using namespace std;
using namespace llvm;

namespace soaap {
  typedef pair<const Value*, Context*> ValueContextPair;
  typedef list<ValueContextPair> ValueContextPairList;
  typedef map<const Value*, int> DataflowFacts;

  class InfoFlowAnalysis : public Analysis {
    public:
      // There are three types of context: no context, privileged context and sandbox
      static Context* const NO_CONTEXT;
      static Context* const PRIV_CONTEXT;
      virtual void doAnalysis(Module& M, SandboxVector& sandboxes);

    protected:
      map<Context*, DataflowFacts> state;
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes) = 0;
      virtual void performDataFlowAnalysis(ValueContextPairList&, SandboxVector& sandboxes, Module& M);
      virtual int performMeet(int fromVal, int toVal);
      virtual bool propagateToValue(const Value* from, const Value* to, Context* cFrom, Context* cTo, Module& M);
      virtual void propagateToCallees(CallInst* CI, const Value* V, Context* C, ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M);
      virtual void propagateToCallers(ReturnInst* RI, const Value* V, Context* C, ValueContextPairList& worklist, SandboxVector& sandboxes, Module& M);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) = 0;
      virtual void addToWorklist(const Value* V, Context* C, ValueContextPairList& worklist);
  };

}

#endif
