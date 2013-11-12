#ifndef SOAAP_ANALYSIS_ANALYSIS_H
#define SOAAP_ANALYSIS_ANALYSIS_H

#include "llvm/IR/Module.h"
#include "Common/Sandbox.h"

using namespace llvm;

namespace soaap {
  class Analysis {
    public:
      virtual void doAnalysis(Module& M, SandboxVector& sandboxes) = 0;
  };
}

#endif
