#ifndef SOAAP_ANALYSIS_INFOFLOW_CAPABILITYSYSCALLSANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_CAPABILITYSYSCALLSANALYSIS_H

#include "Analysis/CFGFlow/SysCallsAnalysis.h"
#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"
#include "OS/FreeBSDSysCallProvider.h"
#include "OS/Sandbox/SandboxPlatform.h"

#include <string>

using namespace llvm;
using namespace std;

namespace soaap {

  class CapabilitySysCallsAnalysis : public InfoFlowAnalysis<BitVector> {
    public:
      CapabilitySysCallsAnalysis(bool contextInsensitive,
                                 shared_ptr<SandboxPlatform>& platform,
                                 shared_ptr<SysCallProvider>& os,
                                 SysCallsAnalysis& analysis)
           : InfoFlowAnalysis<BitVector>(contextInsensitive, true),
             sandboxPlatform(platform),
             operatingSystem(os),
             sysCallsAnalysis(analysis) { }

    protected:
      shared_ptr<SysCallProvider> operatingSystem;
      shared_ptr<SandboxPlatform> sandboxPlatform;
      SysCallsAnalysis& sysCallsAnalysis;
      map<int,BitVector> intFdToAllowedSysCalls;
      map<int,BitVector> fdKeyToAllowedSysCalls;
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual bool performMeet(BitVector fromVal, BitVector& toVal);
      virtual bool performUnion(BitVector fromVal, BitVector& toVal);
      virtual BitVector bottomValue() { return BitVector(); }
      virtual bool checkEqual(BitVector f1, BitVector f2) { return f1 == f2; }
      virtual string stringifyFact(BitVector fact);
      virtual BitVector convertFunctionSetToBitVector(FunctionSet sysCalls);

  };
}

#endif 
