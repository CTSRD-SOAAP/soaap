#ifndef SOAAP_COMMON_SANDBOX_H
#define SOAAP_COMMON_SANDBOX_H

#include "Common/Typedefs.h"
#include "llvm/Analysis/CallGraph.h"

using namespace llvm;

namespace soaap {
  typedef map<GlobalVariable*,int> GlobalVariableIntMap;
  class Sandbox {
    public:
      Sandbox(string n, int i, Function* entry, bool p, Module& m, int o, int c);
      int getNameIdx();
      Function* getEntryPoint();
      FunctionVector getFunctions();
      GlobalVariableIntMap getGlobalVarPerms();
      bool isAllowedToReadGlobalVar(GlobalVariable* gv);

    private:
      Module& module;
      string name;
      int nameIdx;
      Function* entryPoint;
      bool persistent;
      int clearances;
      FunctionVector callgates;
      FunctionVector functions;
      GlobalVariableIntMap sharedVarToPerms;
      int overhead;
      
      void findSandboxedFunctions();
      void findSandboxedFunctionsHelper(CallGraphNode* n);
      void findSharedGlobalVariables();
  };
  typedef SmallVector<Sandbox*,16> SandboxVector;
}

#endif
