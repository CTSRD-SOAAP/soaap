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
      static void reinitSandboxes(SandboxVector& sandboxes);
      static string stringifySandboxNames(int sandboxNames);
      static string stringifySandboxVector(SandboxVector& sandboxes);
      static bool isSandboxEntryPoint(Module& M, Function* F);
      static bool isWithinSandboxedRegion(Instruction* I, SandboxVector& sandboxes);
      static Sandbox* getSandboxForEntryPoint(Function* F, SandboxVector& sandboxes);
      static SandboxVector getSandboxesContainingMethod(Function* F, SandboxVector& sandboxes);
      static SandboxVector getSandboxesContainingInstruction(Instruction* I, SandboxVector& sandboxes);
      static Sandbox* getSandboxWithName(string name, SandboxVector& sandboxes);
      static void outputSandboxedFunctions(SandboxVector& sandboxes);
      static void outputPrivilegedFunctions();
      static bool isSandboxedFunction(Function* F, SandboxVector& sandboxes);
      static SandboxVector convertNamesToVector(int sandboxNames, SandboxVector& sandboxes);
      static void validateSandboxCreations(SandboxVector& sandboxes);
      
      static FunctionSet getPrivilegedMethods(Module& M);
      static void recalculatePrivilegedMethods(Module& M);
      static bool isPrivilegedMethod(Function* F, Module& M);
      static bool isPrivilegedInstruction(Instruction* I, SandboxVector& sandboxes, Module& M);
    
    private:
      static FunctionSet privilegedMethods;
      static map<string,int> sandboxNameToBitIdx;
      static map<int,string> bitIdxToSandboxName;
      static int nextSandboxNameBitIdx;
      static SmallSet<Function*,16> sandboxEntryPoints;
      static void createEmptySandboxIfNew(string name, SandboxVector& sandboxes, Module& M);
      static int assignBitIdxToSandboxName(string sandboxName);
      static void calculateSandboxedMethods(Function* F, Sandbox* S, FunctionVector& sandboxedMethods);
      static void calculatePrivilegedMethods(Module& M);
      static void calculatePrivilegedMethodsHelper(Module& M, Function* F);
      static void findAllSandboxedInstructions(Instruction* I, string sboxName, InstVector& insts);
  };
}

#endif
