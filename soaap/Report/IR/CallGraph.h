#ifndef SOAAP_REPORT_IR_CALLGRAPH_H
#define SOAAP_REPORT_IR_CALLGRAPH_H

#include "Report/IR/Element.h"
#include "Report/Render/Renderer.h"

#include "llvm/IR/Function.h"

#include <map>

using namespace llvm;
using namespace std;

namespace soaap {
  typedef map<Function*,map<Function*,int> > FunctionToCalleeCallCountsMap;
  class CallGraph : public Element {
    public:
      CallGraph(FunctionToCalleeCallCountsMap& map) : funcToCalleeCallCounts(map) { }
      virtual void render(Renderer* r);
      virtual FunctionToCalleeCallCountsMap getFuncToCalleeCallCounts() {
        return funcToCalleeCallCounts;
      }
    
    protected:
      FunctionToCalleeCallCountsMap funcToCalleeCallCounts;
  };
}

#endif
