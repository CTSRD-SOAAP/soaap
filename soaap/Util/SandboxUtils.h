#ifndef SOAAP_UTILS_SANDBOXUTILS_H
#define SOAAP_UTILS_SANDBOXUTILS_H

#include "Common/Typedefs.h"
#include "Common/Sandbox.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/CallGraph.h"
#include <string>
#include <map>

using namespace std;
using namespace llvm;

namespace soaap {
  class SandboxUtils {
    public:
      static SandboxVector findSandboxes(Module& M);
      static FunctionVector getSandboxedMethods(SandboxVector& sandboxes);
      static int assignBitIdxToSandboxName(string sandboxName);
      static int getBitIdxFromSandboxName(string sandboxName);
      static string stringifySandboxNames(int sandboxNames);
      static bool isSandboxEntryPoint(Module& M, Function* F);
      static FunctionVector getPrivilegedMethods(Module& M);
      static bool isPrivilegedMethod(Function* F, Module& M);
      static Sandbox* getSandboxForEntryPoint(Function* F, SandboxVector& sandboxes);
      static SandboxVector getSandboxesContainingMethod(Function* F, SandboxVector& sandboxes);
      static void outputSandboxedFunctions(SandboxVector& sandboxes);
    
    private:
      static map<string,int> sandboxNameToBitIdx;
      static map<int,string> bitIdxToSandboxName;
      static int nextSandboxNameBitIdx;
      static FunctionVector privilegedMethods;
      static void calculateSandboxedMethods(CallGraphNode* node, int sandboxName, Function* entryPoint, FunctionVector& sandboxedMethods);
      static void calculatePrivilegedMethods(Module& M, CallGraphNode* Node);
      static void findAllSandboxedInstructions(Instruction* I, string sboxName, InstVector& insts);
  };
}

#endif
