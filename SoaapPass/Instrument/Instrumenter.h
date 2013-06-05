#ifndef SOAAP_INSTRUMENT_INSTRUMENTER_H
#define SOAAP_INSTRUMENT_INSTRUMENTER_H

#include "Common/Sandbox.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace soaap {
  class Instrumenter {
    public:
      virtual void instrument(Module& M, SandboxVector& sandboxes) = 0;
  };
}

#endif
