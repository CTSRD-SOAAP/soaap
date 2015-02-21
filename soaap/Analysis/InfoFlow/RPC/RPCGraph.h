#ifndef SOAAP_ANALYSIS_INFOFLOW_RPCGRAPH_H
#define SOAAP_ANALYSIS_INFOFLOW_RPCGRAPH_H

#include "Common/Sandbox.h"
#include "Common/Typedefs.h"

#include "llvm/ADT/SmallVector.h"

#include <map>

using namespace std;

namespace soaap {
  typedef std::tuple<CallInst*,string,Sandbox*,Function*> RPCCallRecord;
  class RPCGraph {
    public:
      void build(SandboxVector& sandboxes, FunctionSet& privilegedMethods, Module& M);
      void dump(Module& M);

    private:
      map<Sandbox*,SmallVector<RPCCallRecord, 16>> rpcLinks;
  };
}

#endif
