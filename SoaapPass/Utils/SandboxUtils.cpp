#include "Utils/SandboxUtils.h"
#include "Utils/LLVMAnalyses.h"
#include "soaap.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/GlobalVariable.h"

using namespace soaap;
using namespace llvm;

int SandboxUtils::nextSandboxNameBitIdx = 0;
map<string,int> SandboxUtils::sandboxNameToBitIdx;
map<int,string> SandboxUtils::bitIdxToSandboxName;

string SandboxUtils::stringifySandboxNames(int sandboxNames) {
  string sandboxNamesStr = "[";
  int currIdx = 0;
  bool first = true;
  for (currIdx=0; currIdx<=31; currIdx++) {
    if (sandboxNames & (1 << currIdx)) {
      string sandboxName = bitIdxToSandboxName[currIdx];
      if (!first) 
        sandboxNamesStr += ",";
      sandboxNamesStr += sandboxName;
      first = false;
    }
  }
  sandboxNamesStr += "]";
  return sandboxNamesStr;
}

int SandboxUtils::assignBitIdxToSandboxName(string sandboxName) {
  if (sandboxNameToBitIdx.find(sandboxName) == sandboxNameToBitIdx.end()) {
    dbgs() << "    Assigning index " << nextSandboxNameBitIdx << " to sandbox name \"" << sandboxName << "\"\n";
    sandboxNameToBitIdx[sandboxName] = nextSandboxNameBitIdx;
    bitIdxToSandboxName[nextSandboxNameBitIdx] = sandboxName;
    nextSandboxNameBitIdx++;
    return nextSandboxNameBitIdx-1;
  }
  else {
    dbgs() << "ERROR: sandbox with name " << sandboxName << " already exists!\n";
    return -1;
  }
}

int SandboxUtils::getBitIdxFromSandboxName(string sandboxName) {
  return sandboxNameToBitIdx[sandboxName];
}

SandboxVector SandboxUtils::findSandboxes(Module& M) {
  SandboxVector sandboxes;
  if (GlobalVariable* lga = M.getNamedGlobal("llvm.global.annotations")) {
    ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
    for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
      ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());

      // get the annotation value first
      GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
      ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
      StringRef annotationStrArrayCString = annotationStrArray->getAsCString();

      GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
      if (isa<Function>(annotatedVal)) {
        Function* annotatedFunc = dyn_cast<Function>(annotatedVal);
        if (annotationStrArrayCString.startswith(SANDBOX_PERSISTENT)) {
          outs() << "   Found persistent sandbox entry-point " << annotatedFunc->getName() << "\n";
          
          //persistentSandboxEntryPoints.push_back(annotatedFunc);
          //allSandboxEntryPoints.push_back(annotatedFunc);
          // get name if one was specified
          if (annotationStrArrayCString.size() > strlen(SANDBOX_PERSISTENT)) {
            StringRef sandboxName = annotationStrArrayCString.substr(strlen(SANDBOX_PERSISTENT)+1);
            outs() << "      Sandbox name: " << sandboxName << "\n";
            int idx = assignBitIdxToSandboxName(sandboxName);
            sandboxes.push_back(new Sandbox(sandboxName, idx, annotatedFunc, true));
            //sandboxEntryPointToName[annotatedFunc] = (1 << SandboxUtils::getBitIdxFromSandboxName(sandboxName));
            //DEBUG(dbgs() << "sandboxEntryPointToName[" << annotatedFunc->getName() << "]: " << sandboxEntryPointToName[annotatedFunc] << "\n");
          }
        }
        else if (annotationStrArrayCString.startswith(SANDBOX_EPHEMERAL)) {
          outs() << "   Found ephemeral sandbox entry-point " << annotatedFunc->getName() << "\n";
          sandboxes.push_back(new Sandbox("", -1, annotatedFunc, false));
          //ephemeralSandboxEntryPoints.push_back(annotatedFunc);
          //allSandboxEntryPoints.push_back(annotatedFunc);
        }
        /*
        else if (sboxPerfRegex->match(annotationStrArrayCString, &matches)) {
          int overhead;
          cout << "Threshold set to " << matches[1].str() <<
                  "%\n";
          matches[1].getAsInteger(0, overhead);
          sandboxedMethodToOverhead[annotatedFunc] = overhead;
        }
        else if (annotationStrArrayCString.startswith(CLEARANCE)) {
          StringRef className = annotationStrArrayCString.substr(strlen(CLEARANCE)+1);
          outs() << "   Sandbox has clearance for \"" << className << "\"\n";
          ClassifiedUtils::assignBitIdxToClassName(className);
          sandboxedMethodToClearances[annotatedFunc] |= (1 << ClassifiedUtils::getBitIdxFromClassName(className));
        }
        */
      }
    }
  }
  return sandboxes;
}


FunctionVector SandboxUtils::calculateSandboxedMethods(SandboxVector& sandboxes) {
  FunctionVector sandboxedMethods;
  CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  for (Sandbox* S : sandboxes) {
    Function* F = S->getEntryPoint();
    CallGraphNode* Node = CG->getOrInsertFunction(F);
    int sandboxName = S->getNameIdx();
    calculateSandboxedMethodsHelper(Node, sandboxName, F, sandboxedMethods);
  }
  return sandboxedMethods;
}

void SandboxUtils::calculateSandboxedMethodsHelper(CallGraphNode* node, int sandboxName, Function* entryPoint, FunctionVector& sandboxedMethods) {
  Function* F = node->getFunction();
  DEBUG(dbgs() << "Visiting " << F->getName() << "\n");
   
  if (find(sandboxedMethods.begin(), sandboxedMethods.end(), F) != sandboxedMethods.end()) {
    // cycle detected
    return;
  }

  sandboxedMethods.push_back(F);
  //allReachableMethods.insert(F);
  //sandboxedMethodToClearances[F] |= clearances;

  //if (F != entryPoint) {
    //funcToSandboxEntryPoint[F].push_back(entryPoint);
  //}

  /*
  if (sandboxName != 0) {
    DEBUG(dbgs() << "   Assigning name: " << sandboxName << "\n");
    sandboxedMethodToNames[F] |= sandboxName;
  }
  */

//      cout << "Adding " << node->getFunction()->getName().str() << " to visited" << endl;
  for (CallGraphNode::iterator I=node->begin(), E=node->end(); I != E; I++) {
    Value* V = I->first;
    CallGraphNode* calleeNode = I->second;
    if (Function* calleeFunc = calleeNode->getFunction()) {
      //TODO: If an entry point, update sandboxName and entryPoint
      /*if (sandboxEntryPointToName.find(calleeFunc) != sandboxEntryPointToName.end()) {
        DEBUG(dbgs() << "   Encountered sandbox entry point, changing sandbox name to: " << SandboxUtils::stringifySandboxNames(sandboxName));
        sandboxName = sandboxEntryPointToName[calleeFunc];
        entryPoint = calleeFunc;
      }*/
      calculateSandboxedMethodsHelper(calleeNode, sandboxName, entryPoint, sandboxedMethods);
    }
  }
}
