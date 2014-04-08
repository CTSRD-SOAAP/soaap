#ifndef SOAAP_ANALYSIS_INFOFLOW_ACCESSORIGINANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_ACCESSORIGINANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

namespace soaap {

  class AccessOriginAnalysis : public InfoFlowAnalysis<int> {
    static const int UNINITIALISED = INT_MAX;
    static const int ORIGIN_PRIV = 0;
    static const int ORIGIN_SANDBOX = 1;

    public:
      AccessOriginAnalysis(bool contextInsensitive, FunctionSet& privileged) : InfoFlowAnalysis<int>(contextInsensitive), privilegedMethods(privileged) { }

    protected:
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual bool performMeet(int from, int& to);
      virtual int bottomValue() { return 0; }
      virtual string stringifyFact(int fact);

    private:
      FunctionSet privilegedMethods;
      CallInstVector untrustedSources;

      void ppPrivilegedPathToInstruction(Instruction* I, Module& M);
  };
}
#endif 
