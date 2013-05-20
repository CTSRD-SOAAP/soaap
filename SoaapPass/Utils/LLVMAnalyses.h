#ifndef SOAAP_UTILS_LLVMANALYSES_H
#define SOAAP_UTILS_LLVMANALYSES_H

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/ProfileInfo.h"

using namespace llvm;

namespace soaap {
  class LLVMAnalyses {
    public:
      static CallGraph* getCallGraphAnalysis();
      static ProfileInfo* getProfileInfoAnalysis();
      static void setCallGraphAnalysis(CallGraph* graph);
      static void setProfileInfoAnalysis(ProfileInfo* info);

    private:
      static CallGraph* CG;
      static ProfileInfo* PI;
  };
}

#endif
