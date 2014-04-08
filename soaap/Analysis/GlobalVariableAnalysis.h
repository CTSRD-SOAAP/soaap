#ifndef SOAAP_ANALYSIS_GLOBALVARIABLEANALYSIS_H
#define SOAAP_ANALYSIS_GLOBALVARIABLEANALYSIS_H

#include "Analysis/Analysis.h"
#include "llvm/ADT/SetVector.h"

namespace soaap {

  class GlobalVariableAnalysis : public Analysis {
    public:
      GlobalVariableAnalysis(FunctionSet& privMethods) : privilegedMethods(privMethods) { }
      virtual void doAnalysis(Module& M, SandboxVector& sandboxes);
    
    private:
      FunctionSet privilegedMethods;
      string findGlobalDeclaration(Module& M, GlobalVariable* G);
      void checkSharedGlobalWrites(Module& M, SandboxVector& sandboxes, map<GlobalVariable*,SandboxVector>& varToSandboxes);
      void updateReachingCreationsStateAndPropagate(map<Instruction*,int>& state, Instruction* I, int val, SetVector<BasicBlock*>& worklist);
  };

}

#endif
