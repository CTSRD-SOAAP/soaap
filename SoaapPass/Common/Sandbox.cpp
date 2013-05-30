#include "Common/Sandbox.h"
#include "Utils/LLVMAnalyses.h"
#include "Utils/SandboxUtils.h"
#include "soaap.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;

Sandbox::Sandbox(string n, int i, Function* entry, bool p, Module& m) 
  : name(n), nameIdx(i), entryPoint(entry), persistent(p), module(m) {
  findSandboxedFunctions();
  findSharedGlobalVariables();
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

GlobalVariableIntMap Sandbox::getVariablePermissions() {
  return sharedVarToPerms;
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

/*
 * Find global variables that are annotated as being shared with this 
 * sandbox and record how they are allowed to be accessed
 */
void Sandbox::findSharedGlobalVariables() {

  /*
   * Global variable annotations are added to the global intrinsic
   * array called llvm.global.annotations:
   *
   * int fd __attribute__((annotate("var_read")));
   *
   * @fd = common global i32 0, align 4
   * @.str = private unnamed_addr constant [9 x i8] c"var_read\00", section "llvm.metadata"
   * @.str1 = private unnamed_addr constant [7 x i8] c"test-var.c\00", section "llvm.metadata"
   * @llvm.global.annotations = appending global [1 x { i8*, i8*, i8*, i32 }]
   *    [{ i8*, i8*, i8*, i32 }
   *     { i8* bitcast (i32* @fd to i8*),   // annotated global variable
   *       i8* getelementptr inbounds ([9 x i8]* @.str, i32 0, i32 0),  // annotation
   *       i8* getelementptr inbounds ([7 x i8]* @.str1, i32 0, i32 0), // file name
   *       i32 1 }] // line number
   */
  GlobalVariable* lga = module.getNamedGlobal("llvm.global.annotations");
  if (lga != NULL) {
    ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
    for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
      ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());

      // get the annotation value first
      GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
      ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
      StringRef annotationStrArrayCString = annotationStrArray->getAsCString();

      GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
      if (isa<GlobalVariable>(annotatedVal)) {
        GlobalVariable* annotatedVar = dyn_cast<GlobalVariable>(annotatedVal);
        if (annotationStrArrayCString.startswith(VAR_READ)) {
          StringRef sandboxName = annotationStrArrayCString.substr(strlen(VAR_READ)+1);
          if (sandboxName == name) {
            sharedVarToPerms[annotatedVar] |= VAR_READ_MASK;
          }
          dbgs() << "   Found annotated global var " << annotatedVar->getName() << "\n";
        }
        else if (annotationStrArrayCString.startswith(VAR_WRITE)) {
          StringRef sandboxName = annotationStrArrayCString.substr(strlen(VAR_WRITE)+1);
          if (sandboxName == name) {
            sharedVarToPerms[annotatedVar] |= VAR_WRITE_MASK;
          }
          dbgs() << "   Found annotated global var " << annotatedVar->getName() << "\n";
        }
      }
    }
  }

}
