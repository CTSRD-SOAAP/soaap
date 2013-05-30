#include "Common/Sandbox.h"
#include "Utils/LLVMAnalyses.h"
#include "Utils/SandboxUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;

Sandbox::Sandbox(string n, int i, Function* entry, bool p, Module& m) 
  : name(n), nameIdx(i), entryPoint(entry), persistent(p), module(m) {
  findSandboxedFunctions();
}

Function* Sandbox::getEntryPoint() {
  return entryPoint;
}

int Sandbox::getNameIdx() {
  return nameIdx;
}

FunctionVector Sandbox::getFunctions() {
  return functions;
}

void Sandbox::findSandboxedFunctions() {
  CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  CallGraphNode* node = CG->getOrInsertFunction(entryPoint);
  findSandboxedFunctionsHelper(node);
}

void Sandbox::findSandboxedFunctionsHelper(CallGraphNode* node) {
  Function* F = node->getFunction();
  DEBUG(dbgs() << "Visiting " << F->getName() << "\n");
   
  // check for cycle
  if (find(functions.begin(), functions.end(), F) != functions.end()) {
    return;
  }

  // check if entry point to another sandbox
  if (SandboxUtils::isSandboxEntryPoint(module, F) && F != entryPoint) {
    return;
  }

  functions.push_back(F);

//  outs() << "Adding " << node->getFunction()->getName().str() << " to visited" << endl;
  for (CallGraphNode::iterator I=node->begin(), E=node->end(); I != E; I++) {
    Value* V = I->first;
    CallGraphNode* calleeNode = I->second;
    if (Function* calleeFunc = calleeNode->getFunction()) {
      findSandboxedFunctionsHelper(calleeNode);
    }
  }
}
