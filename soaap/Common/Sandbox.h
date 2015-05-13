#ifndef SOAAP_COMMON_SANDBOX_H
#define SOAAP_COMMON_SANDBOX_H

#include "Analysis/InfoFlow/Context.h"
#include "Common/Typedefs.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/CallGraph.h"

using namespace llvm;

namespace soaap {
  typedef map<GlobalVariable*,int> GlobalVariableIntMap;
  class Sandbox : public Context {
    public:
      Sandbox(string n, int i, FunctionSet entries, bool p, Module& m, int o, int c);
      Sandbox(string n, int i, InstVector& region, bool p, Module& m);
      string getName();
      int getNameIdx();
      FunctionSet getEntryPoints();
      Function* getEnclosingFunc();
      bool isRegionWithin(Function* F);
      FunctionVector getFunctions();
      CallInstVector getCalls();
      CallInstVector getTopLevelCalls();
      InstVector getRegion();
      GlobalVariableIntMap getGlobalVarPerms();
      ValueFunctionSetMap getCapabilities();
      bool isAllowedToReadGlobalVar(GlobalVariable* gv);
      FunctionVector getCallgates();
      bool isCallgate(Function* F);
      bool isEntryPoint(Function* F);
      int getClearances();
      int getOverhead();
      bool isPersistent();
      CallInstVector getCreationPoints();
      CallInstVector getSysCallLimitPoints();
      FunctionSet getAllowedSysCalls(CallInst* sysCallLimitPoint);
      ValueSet getPrivateData();
      bool containsFunction(Function* F);
      bool containsInstruction(Instruction* I);
      bool hasCallgate(Function* F);
      void validateCreationPoints();
      void reinit();
      static bool classof(const Context* C) { return C->getKind() == CK_SANDBOX; }

    private:
      Module& module;
      string name;
      int nameIdx;
      FunctionSet entryPoints;
      InstVector region;
      bool persistent;
      int clearances;
      FunctionVector callgates;
      FunctionVector functionsVec;
      DenseSet<Function*> functionsSet;
      CallInstVector tlCallInsts;
      CallInstVector callInsts;
      CallInstVector creationPoints;
      CallInstVector sysCallLimitPoints;
      map<CallInst*,FunctionSet> sysCallLimitPointToAllowedSysCalls;
      GlobalVariableIntMap sharedVarToPerms;
      ValueFunctionSetMap caps;
      int overhead;
      ValueSet privateData;
      
      void init();
      void findSandboxedFunctions();
      void findSandboxedFunctionsHelper(FunctionSet funcs);
      void findSandboxedCalls();
      void findSharedGlobalVariables();
      void findCallgates();
      void findCapabilities();
      void findAllowedSysCalls();
      void findCreationPoints();
      void findPrivateData();
      bool validateCreationPointsHelper(BasicBlock* BB, BasicBlockVector& visited, InstTrace& trace);
  };
  typedef SmallVector<Sandbox*,16> SandboxVector;
}

#endif
