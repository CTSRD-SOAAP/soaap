#ifndef SOAAP_ANALYSIS_INFOFLOW_CAPABILITYSYSCALLSANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_CAPABILITYSYSCALLSANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"
#include "OS/FreeBSDSysCallProvider.h"

#include <string>

using namespace llvm;
using namespace std;

namespace soaap {

  class CapabilitySysCallsAnalysis : public InfoFlowAnalysis<BitVector> {
    public:
      CapabilitySysCallsAnalysis(bool contextInsensitive) : InfoFlowAnalysis<BitVector>(contextInsensitive, true) { }

    protected:
      FreeBSDSysCallProvider freeBSDSysCallProvider;
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual bool performMeet(BitVector fromVal, BitVector& toVal);
      virtual BitVector bottomValue() { return BitVector(); }
      virtual string stringifyFact(BitVector fact);
      virtual BitVector convertFunctionSetToBitVector(FunctionSet sysCalls);

  };
}

#endif 
