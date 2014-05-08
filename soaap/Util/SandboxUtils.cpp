#include "Common/Debug.h"
#include "Util/ClassifiedUtils.h"
#include "Util/DebugUtils.h"
#include "Util/SandboxUtils.h"
#include "Util/LLVMAnalyses.h"
#include "soaap.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Regex.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IntrinsicInst.h"

using namespace soaap;
using namespace llvm;

int SandboxUtils::nextSandboxNameBitIdx = 0;
map<string,int> SandboxUtils::sandboxNameToBitIdx;
map<int,string> SandboxUtils::bitIdxToSandboxName;
FunctionSet SandboxUtils::privilegedMethods;
SmallSet<Function*,16> SandboxUtils::sandboxEntryPoints;

string SandboxUtils::stringifySandboxNames(int sandboxNames) {
  string sandboxNamesStr = "[";
  int currIdx = 0;
  bool first = true;
  for (currIdx=0; currIdx<=31; currIdx++) {
    if (sandboxNames & (1 << currIdx)) {
      string sandboxName = bitIdxToSandboxName[currIdx];
      if (!first) {
        sandboxNamesStr += ",";
      }
      sandboxNamesStr += sandboxName;
      first = false;
    }
  }
  sandboxNamesStr += "]";
  return sandboxNamesStr;
}

string SandboxUtils::stringifySandboxVector(SandboxVector& sandboxes) {
  string sandboxNamesStr = "[";
  bool first = true;
  for (Sandbox* S : sandboxes) {
    if (!first) {
      sandboxNamesStr += ",";
    }
    sandboxNamesStr += S->getName();
    first = false;
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
  return sandboxEntryPoints.count(F);
}

SandboxVector SandboxUtils::findSandboxes(Module& M) {
  SandboxVector sandboxes;
  FunctionIntMap funcToOverhead;
  FunctionIntMap funcToClearances;
  map<Function*,string> funcToSandboxName;
  FunctionVector ephemeralSandboxes;

  // function-level annotations of sandboxed code
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
        StringRef sandboxName;
        if (annotationStrArrayCString.startswith(SANDBOX_PERSISTENT) || annotationStrArrayCString.startswith(SANDBOX_EPHEMERAL)) {
          sandboxEntryPoints.insert(annotatedFunc);
          outs() << INDENT_1 << "Found sandbox entrypoint " << annotatedFunc->getName() << "\n";
          outs() << INDENT_2 << "Annotation string: " << annotationStrArrayCString << "\n";
          if (annotationStrArrayCString.startswith(SANDBOX_PERSISTENT)) {
            sandboxName = annotationStrArrayCString.substr(strlen(SANDBOX_PERSISTENT)+1);
          }
          else if (annotationStrArrayCString.startswith(SANDBOX_EPHEMERAL)) {
            sandboxName = annotationStrArrayCString.substr(strlen(SANDBOX_EPHEMERAL)+1);
            ephemeralSandboxes.push_back(annotatedFunc);
          }
          outs() << INDENT_2 << "Sandbox name: " << sandboxName << "\n";
          if (funcToSandboxName.find(annotatedFunc) != funcToSandboxName.end()) {
            outs() << INDENT_1 << "*** Error: Function " << annotatedFunc->getName() << " is already an entrypoint for another sandbox\n";
          }
          else {
            funcToSandboxName[annotatedFunc] = sandboxName;
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

  // Combine all annotation information for function-level sandboxes to create Sandbox instances
  for (map<Function*,string>::iterator I=funcToSandboxName.begin(), E=funcToSandboxName.end(); I!=E; I++) {
    Function* entryPoint = I->first;
    string sandboxName = I->second;
    int idx = assignBitIdxToSandboxName(sandboxName);
    int overhead = funcToOverhead[entryPoint];
    int clearances = funcToClearances[entryPoint];
    bool persistent = find(ephemeralSandboxes.begin(), ephemeralSandboxes.end(), entryPoint) == ephemeralSandboxes.end();
		SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Creating new Sandbox instance\n");
    sandboxes.push_back(new Sandbox(sandboxName, idx, entryPoint, persistent, M, overhead, clearances));
		SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_2 << "Created new Sandbox instance\n");
  }

  // Handle sandboxe code regions, i.e. start_sandboxed_code(N) and end_sandboxed_code(N) blocks 
  if (Function* SboxStart = M.getFunction("llvm.annotation.i32")) {
    for (User* U : SboxStart->users()) {
      if (IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U)) {
        GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
        ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
        StringRef annotationStrValCString = annotationStrValArray->getAsCString();
        
        if (annotationStrValCString.startswith(SOAAP_SANDBOX_REGION_START)) {
          StringRef sandboxName = annotationStrValCString.substr(strlen(SOAAP_SANDBOX_REGION_START)+1); //+1 because of _
          SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Found start of sandboxed code region: "; annotateCall->dump(););
          InstVector sandboxedInsts;
          findAllSandboxedInstructions(annotateCall, sandboxName, sandboxedInsts);
          int idx = assignBitIdxToSandboxName(sandboxName);
          sandboxes.push_back(new Sandbox(sandboxName, idx, sandboxedInsts, false, M)); //TODO: obtain persistent/ephemeral information in a better way (currently we obtain it from the creation point)
        }
      }
    }
  }

	SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_1 << "Returning sandboxes vector\n");
  return sandboxes;
}

void SandboxUtils::findAllSandboxedInstructions(Instruction* I, string startSandboxName, InstVector& insts) {
  BasicBlock* BB = I->getParent();
  
  // If I is not the start of BB, then fast-forward iterator to it
  BasicBlock::iterator BI = BB->begin();
  while (&*BI != I) { BI++; }

  for (BasicBlock::iterator BE = BB->end(); BI != BE; BI++) {
    // If I is the end_sandboxed_code() annotation then we stop
    I = &*BI;
    SDEBUG("soaap.util.sandbox", 3, I->dump());
    insts.push_back(I);
    if (isa<IntrinsicInst>(I)) {
      GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(I->getOperand(1)->stripPointerCasts());
      ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
      StringRef annotationStrValCString = annotationStrValArray->getAsCString();
      
      if (annotationStrValCString.startswith(SOAAP_SANDBOX_REGION_END)) {
        StringRef endSandboxName = annotationStrValCString.substr(strlen(SOAAP_SANDBOX_REGION_END)+1); //+1 because of _
        SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_3 << "Found end of sandboxed code region: "; I->dump());
        if (endSandboxName == startSandboxName) {
          // we have found the end of the region
          return;
        }
      }
    }
  }

  // recurse on successor BBs
  for (succ_iterator SI = succ_begin(BB), SE = succ_end(BB); SI != SE; SI++) {
    BasicBlock* SBB = *SI;
    findAllSandboxedInstructions(SBB->begin(), startSandboxName, insts);
  }
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
  SDEBUG("soaap.util.sandbox", 3, dbgs() << "Visiting " << F->getName() << "\n");
   
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
        SDEBUG("soaap.util.sandbox", 3, dbgs() << "   Encountered sandbox entry point, changing sandbox name to: " << SandboxUtils::stringifySandboxNames(sandboxName));
        sandboxName = sandboxEntryPointToName[calleeFunc];
        entryPoint = calleeFunc;
      }*/
      calculateSandboxedMethods(calleeNode, sandboxName, entryPoint, sandboxedMethods);
    }
  }
}

FunctionSet SandboxUtils::getPrivilegedMethods(Module& M) {
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
    if (privilegedMethods.count(F) > 0)
      return;

    SDEBUG("soaap.util.sandbox", 3, dbgs() << INDENT_1 << "Added " << F->getName() << " as privileged method\n");
    privilegedMethods.insert(F);

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
  return privilegedMethods.count(F) > 0;
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

Sandbox* SandboxUtils::getSandboxWithName(string name, SandboxVector& sandboxes) {
  for (Sandbox* S : sandboxes) {
    if (S->getName() == name) {
      return S;
    }
  }
  return NULL;
}

void SandboxUtils::outputSandboxedFunctions(SandboxVector& sandboxes) {
  for (Sandbox* S : sandboxes) {
    outs() << INDENT_1 << "Sandbox: " << S->getName() << " (" << (S->isPersistent() ? "persistent" : "ephemeral") << ")\n";
    for (Function* F : S->getFunctions()) {
      if (F->isDeclaration()) { continue; }
      outs() << INDENT_2 << F->getName();
      // get filename of file
      Instruction* I = F->getEntryBlock().getTerminator();
      //dbgs() << INDENT_3 << "I: " << *I << "\n";
      if (MDNode *N = I->getMetadata("dbg")) {
        DILocation loc(N);
        outs() << " (" << loc.getFilename().str() << ")";
      }
      outs() << "\n";
    }
    outs() << "\n";
  }
}

bool SandboxUtils::isSandboxedFunction(Function* F, SandboxVector& sandboxes) {
  for (Sandbox* S : sandboxes) {
    if (S->containsFunction(F)) {
      return true;
    }
  }
  return false;
}
