#include "Util/LLVMAnalyses.h"

using namespace soaap;

CallGraph* LLVMAnalyses::CG = NULL;

CallGraph* LLVMAnalyses::getCallGraphAnalysis() {
  return CG;
}

void LLVMAnalyses::setCallGraphAnalysis(CallGraph* graph) {
  CG = graph;
}

