#ifndef SOAAP_ANALYSIS_INFOFLOW_CAPABILITYANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_CAPABILITYANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"
#include "OS/FreeBSDSysCallProvider.h"

#include <string>

using namespace llvm;
using namespace std;

namespace soaap {

  class CapabilityAnalysis : public InfoFlowAnalysis<BitVector> {
    public:
      CapabilityAnalysis(bool contextInsensitive) : InfoFlowAnalysis<BitVector>(contextInsensitive, true) { }

    protected:
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual bool performMeet(BitVector fromVal, BitVector& toVal);
      virtual bool performUnion(BitVector fromVal, BitVector& toVal);
      virtual BitVector bottomValue() { return BitVector(); }
      virtual string stringifyFact(BitVector vector);

    private:
      FreeBSDSysCallProvider freeBSDSysCallProvider;
      void validateDescriptorAccesses(Module& M, SandboxVector& sandboxes, string syscall, int requiredPerm);
  };
}

#endif 
