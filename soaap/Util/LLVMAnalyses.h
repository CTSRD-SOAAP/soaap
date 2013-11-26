#ifndef SOAAP_UTILS_LLVMANALYSES_H
#define SOAAP_UTILS_LLVMANALYSES_H

#include "llvm/Analysis/CallGraph.h"

using namespace llvm;

namespace soaap {
  class LLVMAnalyses {
    public:
      static CallGraph* getCallGraphAnalysis();
      static void setCallGraphAnalysis(CallGraph* graph);

    private:
      static CallGraph* CG;
  };
}

#endif
