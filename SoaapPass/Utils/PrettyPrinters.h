#ifndef SOAAP_UTILS_PRETTYPRINTERS_H
#define SOAAP_UTILS_PRETTYPRINTERS_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "Common/Typedefs.h"
#include <list>

using namespace llvm;
using namespace std;

namespace soaap {
  class PrettyPrinters {
    public:
      static void ppPrivilegedPathToFunction(Function* Target, Module& M);
      static void ppTaintSource(CallInst* C);
      static void ppTrace(list<Instruction*>& trace);
  };
}

#endif
