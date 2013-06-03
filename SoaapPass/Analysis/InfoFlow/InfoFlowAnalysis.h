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
  typedef list<const Value*> ValueList;

  class InfoFlowAnalysis : public Analysis {
    public:
      virtual void doAnalysis(Module& M, SandboxVector& sandboxes);
    protected:
      map<const Value*, int> state;
      virtual void initialise(ValueList& worklist, Module& M, SandboxVector& sandboxes) = 0;
      virtual void performDataFlowAnalysis(ValueList&, Module& M);
      virtual int performMeet(int fromVal, int toVal);
      virtual bool propagateToValue(const Value* from, const Value* to, Module& M);
      virtual void propagateToCallee(const CallInst* CI, const Function* callee, ValueList& worklist, const Value* V, Module& M);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) = 0;
  };

}

#endif
