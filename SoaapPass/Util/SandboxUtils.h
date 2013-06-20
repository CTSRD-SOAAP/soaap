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
      static FunctionVector calculateSandboxedMethods(SandboxVector& sandboxes);
      static int assignBitIdxToSandboxName(string sandboxName);
      static int getBitIdxFromSandboxName(string sandboxName);
      static string stringifySandboxNames(int sandboxNames);
      static bool isSandboxEntryPoint(Module& M, Function* F);
      static FunctionVector calculatePrivilegedMethods(Module& M);
    
    private:
      static map<string,int> sandboxNameToBitIdx;
      static map<int,string> bitIdxToSandboxName;
      static int nextSandboxNameBitIdx;
      static void calculateSandboxedMethodsHelper(CallGraphNode* node, int sandboxName, Function* entryPoint, FunctionVector& sandboxedMethods);
      static void calculatePrivilegedMethodsHelper(Module& M, CallGraphNode* Node, FunctionVector& privilegedMethods);
  };
}

#endif
