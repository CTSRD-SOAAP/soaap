#ifndef SOAAP_UTILS_CALLGRAPHUTILS_H
#define SOAAP_UTILS_CALLGRAPHUTILS_H

#include "llvm/IR/Module.h"

using namespace llvm;

namespace soaap {
  class CallGraphUtils {
    public:
      static void loadDynamicCallGraphEdges(Module& M);
  };
}

#endif
