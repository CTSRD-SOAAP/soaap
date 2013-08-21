#include "Util/ClassifiedUtils.h"
#include "Util/DebugUtils.h"
#include "Util/SandboxUtils.h"
#include "Util/LLVMAnalyses.h"
#include "soaap.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Regex.h"
#include "llvm/IR/GlobalVariable.h"

using namespace soaap;
using namespace llvm;

int SandboxUtils::nextSandboxNameBitIdx = 0;
map<string,int> SandboxUtils::sandboxNameToBitIdx;
map<int,string> SandboxUtils::bitIdxToSandboxName;
FunctionVector SandboxUtils::privilegedMethods;

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

bool SandboxUtils::isSandboxEntryPoint(Module& M, Function* F) {
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
        if (annotationStrArrayCString.startswith(SANDBOX_PERSISTENT) || annotationStrArrayCString.startswith(SANDBOX_EPHEMERAL)) {
          //DEBUG(dbgs() << "   Found sandbox entrypoint " << annotatedFunc->getName() << "\n");
          if (annotatedFunc == F) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

SandboxVector SandboxUtils::findSandboxes(Module& M) {
  SandboxVector sandboxes;
  FunctionIntMap funcToOverhead;
  FunctionIntMap funcToClearances;
  map<Function*,string> funcToPersistentSandboxName;
  FunctionVector ephemeralSandboxes;

  Regex *sboxPerfRegex = new Regex("perf_overhead_\\(([0-9]{1,2})\\)", true);
  SmallVector<StringRef, 4> matches;
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
          outs() << INDENT_1 << "Found persistent sandbox entrypoint " << annotatedFunc->getName() << "\n";
          // get name if one was specified
          if (annotationStrArrayCString.size() > strlen(SANDBOX_PERSISTENT)) {
            StringRef sandboxName = annotationStrArrayCString.substr(strlen(SANDBOX_PERSISTENT)+1);
            outs() << INDENT_2 << "Sandbox name: " << sandboxName << "\n";
            if (funcToPersistentSandboxName.find(annotatedFunc) != funcToPersistentSandboxName.end() || find(ephemeralSandboxes.begin(), ephemeralSandboxes.end(), annotatedFunc) != ephemeralSandboxes.end()) {
              outs() << INDENT_1 << "*** Error: Function " << annotatedFunc->getName() << " is already an entrypoint for another sandbox\n";
            }
            else {
              funcToPersistentSandboxName[annotatedFunc] = sandboxName;
            }
          }
        }
        else if (annotationStrArrayCString.startswith(SANDBOX_EPHEMERAL)) {
          outs() << INDENT_1 << "Found ephemeral sandbox entry-point " << annotatedFunc->getName() << "\n";
          if (funcToPersistentSandboxName.find(annotatedFunc) != funcToPersistentSandboxName.end() || find(ephemeralSandboxes.begin(), ephemeralSandboxes.end(), annotatedFunc) != ephemeralSandboxes.end()) {
            outs() << INDENT_1 << "*** Error: Function " << annotatedFunc->getName() << " is already an entrypoint for another sandbox\n";
          }
          else {
            ephemeralSandboxes.push_back(annotatedFunc);
          }
        }
        else if (sboxPerfRegex->match(annotationStrArrayCString, &matches)) {
          int overhead;
          outs() << INDENT_2 << "Threshold set to " << matches[1].str() <<
                  "%\n";
          matches[1].getAsInteger(0, overhead);
          funcToOverhead[annotatedFunc] = overhead;
        }
        else if (annotationStrArrayCString.startswith(CLEARANCE)) {
          StringRef className = annotationStrArrayCString.substr(strlen(CLEARANCE)+1);
          outs() << INDENT_2 << "Sandbox has clearance for \"" << className << "\"\n";
          ClassifiedUtils::assignBitIdxToClassName(className);
          funcToClearances[annotatedFunc] |= (1 << ClassifiedUtils::getBitIdxFromClassName(className));
        }
      }
    }
  }

  // TODO: sanity check overhead and clearance annotations

  // now combine all annotation information to create Sandbox instances
  for (map<Function*,string>::iterator I=funcToPersistentSandboxName.begin(), E=funcToPersistentSandboxName.end(); I!=E; I++) {
    Function* entryPoint = I->first;
    string sandboxName = I->second;
    int idx = assignBitIdxToSandboxName(sandboxName);
    int overhead = funcToOverhead[entryPoint];
    int clearances = funcToClearances[entryPoint];
		DEBUG(dbgs() << INDENT_2 << "Creating new Sandbox instance\n");
    sandboxes.push_back(new Sandbox(sandboxName, idx, entryPoint, true, M, overhead, clearances));
		DEBUG(dbgs() << INDENT_2 << "Created new Sandbox instance\n");
  }
  for (Function* entryPoint : ephemeralSandboxes) {
    int overhead = funcToOverhead[entryPoint];
    int clearances = funcToClearances[entryPoint];
    sandboxes.push_back(new Sandbox("", -1, entryPoint, false, M, overhead, clearances));
  }

	DEBUG(dbgs() << INDENT_1 << "Returning sandboxes vector\n");
  return sandboxes;
}

FunctionVector SandboxUtils::getSandboxedMethods(SandboxVector& sandboxes) {
  FunctionVector sandboxedMethods;
  CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  for (Sandbox* S : sandboxes) {
    Function* F = S->getEntryPoint();
    CallGraphNode* Node = CG->getOrInsertFunction(F);
    int sandboxName = S->getNameIdx();
    calculateSandboxedMethods(Node, sandboxName, F, sandboxedMethods);
  }
  return sandboxedMethods;
}

void SandboxUtils::calculateSandboxedMethods(CallGraphNode* node, int sandboxName, Function* entryPoint, FunctionVector& sandboxedMethods) {
  Function* F = node->getFunction();
  DEBUG(dbgs() << "Visiting " << F->getName() << "\n");
   
  if (find(sandboxedMethods.begin(), sandboxedMethods.end(), F) != sandboxedMethods.end()) {
    // cycle detected
    return;
  }

  sandboxedMethods.push_back(F);

//      outs() << "Adding " << node->getFunction()->getName().str() << " to visited" << endl;
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
      calculateSandboxedMethods(calleeNode, sandboxName, entryPoint, sandboxedMethods);
    }
  }
}

FunctionVector SandboxUtils::getPrivilegedMethods(Module& M) {
  if (privilegedMethods.empty()) {
    CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
    if (Function* MainFunc = M.getFunction("main")) {
      CallGraphNode* MainNode = (*CG)[MainFunc];
      calculatePrivilegedMethods(M, MainNode);
    }
  }
  return privilegedMethods;
}

void SandboxUtils::calculatePrivilegedMethods(Module& M, CallGraphNode* Node) {
  if (Function* F = Node->getFunction()) {
    // if a sandbox entry point, then ignore
    if (isSandboxEntryPoint(M, F))
      return;
    
    // if already visited this function, then ignore as cycle detected
    if (find(privilegedMethods.begin(), privilegedMethods.end(), F) != privilegedMethods.end())
      return;

    DEBUG(dbgs() << INDENT_1 << "Added " << F->getName() << " as privileged method\n");
    privilegedMethods.push_back(F);

    // recurse on callees
    for (CallGraphNode::iterator I=Node->begin(), E=Node->end(); I!=E; I++) {
      calculatePrivilegedMethods(M, I->second);
    }
  }
}

bool SandboxUtils::isPrivilegedMethod(Function* F, Module& M) {
  if (privilegedMethods.empty()) {
    getPrivilegedMethods(M); // force calculation
  }
  return find(privilegedMethods.begin(), privilegedMethods.end(), F) != privilegedMethods.end();
}

Sandbox* SandboxUtils::getSandboxForEntryPoint(Function* F, SandboxVector& sandboxes) {
  for (Sandbox* S : sandboxes) {
    if (S->getEntryPoint() == F) {
      return S;
    }
  }
  dbgs() << "Could not find sandbox for entrypoint " << F->getName() << "\n";
  return NULL;
}

SandboxVector SandboxUtils::getSandboxesContainingMethod(Function* F, SandboxVector& sandboxes) {
  SandboxVector containers;
  for (Sandbox* S : sandboxes) {
    FunctionVector sFuncs = S->getFunctions();
    if (find(sFuncs.begin(), sFuncs.end(), F) != sFuncs.end()) {
      containers.push_back(S);
    }
  }
  return containers;
}
