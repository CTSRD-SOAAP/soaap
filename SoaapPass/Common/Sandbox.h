#ifndef SOAAP_COMMON_SANDBOX_H
#define SOAAP_COMMON_SANDBOX_H

#include "Common/Typedefs.h"
#include "llvm/Analysis/CallGraph.h"

using namespace llvm;

namespace soaap {
  class Sandbox {
    public:
      Sandbox(string n, int i, Function* entry, bool p, Module& m);
      int getNameIdx();
      Function* getEntryPoint();
      FunctionVector getFunctions();

    private:
      Module& module;
      string name;
      int nameIdx;
      Function* entryPoint;
      bool persistent;
      int clearances;
      FunctionVector callgates;
      FunctionVector functions;
      map<GlobalVariable*,int> sharedVarToPerms;
      
      void findSandboxedFunctions();
      void findSandboxedFunctionsHelper(CallGraphNode* n);
      void findSharedGlobalVariables();
  };
  typedef SmallVector<Sandbox*,16> SandboxVector;
}

#endif
