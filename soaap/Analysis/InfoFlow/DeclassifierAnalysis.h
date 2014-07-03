#ifndef SOAAP_ANALYSIS_INFOFLOW_DECLASSIFIERANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_DECLASSIFIERANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

#include <map>

using namespace llvm;
using namespace std;

namespace soaap {

  class DeclassifierAnalysis: public InfoFlowAnalysis<bool> {
    public:
      DeclassifierAnalysis() : InfoFlowAnalysis<bool>(true, true) { }
      virtual bool isDeclassified(const Value* V);

    protected:
      map<Value*, InstVector> valueToDeclassifiedRegion;
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual bool performMeet(bool from, bool& to);
      virtual bool performUnion(bool from, bool& to);
      virtual bool bottomValue() { return false; }
      virtual string stringifyFact(bool fact);
      virtual void findAllFollowingInstructions(Instruction* I, Value* V);
  };
}

#endif 
