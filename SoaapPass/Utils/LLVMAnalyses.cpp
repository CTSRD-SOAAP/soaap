#include "Utils/LLVMAnalyses.h"

using namespace soaap;

CallGraph* LLVMAnalyses::CG = NULL;
ProfileInfo* LLVMAnalyses::PI = NULL;

CallGraph* LLVMAnalyses::getCallGraphAnalysis() {
  return CG;
}

ProfileInfo* LLVMAnalyses::getProfileInfoAnalysis() {
  return PI;
}

void LLVMAnalyses::setCallGraphAnalysis(CallGraph* graph) {
  CG = graph;
}

void LLVMAnalyses::setProfileInfoAnalysis(ProfileInfo* info) {
  PI = info;
}

