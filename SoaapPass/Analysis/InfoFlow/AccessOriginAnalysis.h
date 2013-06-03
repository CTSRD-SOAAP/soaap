#ifndef SOAAP_ANALYSIS_INFOFLOW_ACCESSORIGINANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_ACCESSORIGINANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

namespace soaap {

  class AccessOriginAnalysis : public InfoFlowAnalysis {
    static const int UNINITIALISED = INT_MAX;
    static const int ORIGIN_PRIV = 0;
    static const int ORIGIN_SANDBOX = 1;

    public:
      AccessOriginAnalysis(FunctionVector& privileged, FunctionVector& sandboxEntries) :
            privilegedMethods(privileged), sandboxEntryPoints(sandboxEntries) { }
      virtual void initialise(ValueList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);

    private:
      FunctionVector privilegedMethods;
      FunctionVector sandboxEntryPoints;
      CallInstVector untrustedSources;

      void ppPrivilegedPathToInstruction(Instruction* I, Module& M);
  };
}
#endif 
