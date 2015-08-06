#include "Util/DebugUtils.h"

#include "Common/Debug.h"

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;

map<Function*, string> DebugUtils::funcToLib;
bool DebugUtils::cachingDone = false;

void DebugUtils::cacheLibraryMetadata(Module* M) {
  if (NamedMDNode* N = M->getNamedMetadata("llvm.libs")) {
    SDEBUG("soaap.util.debug", 3, dbgs() << "Found llvm.libs metadata, " << N->getNumOperands() << " operands:\n");
    for (int i=0; i<N->getNumOperands(); i++) {
      MDNode* lib = N->getOperand(i);
      MDString* name = cast<MDString>(lib->getOperand(0).get());
      string nameStr = name->getString().str();
      SDEBUG("soaap.util.debug", 3, dbgs() << "Processing lib " << nameStr << "\n");
      MDTuple* cus = cast<MDTuple>(lib->getOperand(1).get());
      for (int j=0; j<cus->getNumOperands(); j++) {
        DICompileUnit* cu = cast<DICompileUnit>(cus->getOperand(j).get());
        DISubprogramArray funcs = cu->getSubprograms();
        for (int k=0; k<funcs.size(); k++) {
          DISubprogram* func = funcs[k];
          if (Function* F = func->getFunction()) {
            SDEBUG("soaap.util.debug", 4, dbgs() << INDENT_1 << "Found func: " << F->getName() << "\n");
            if (funcToLib.find(F) != funcToLib.end()) {
              SDEBUG("soaap.util.debug", 3, dbgs() << "WARNING: Function "
                                                   << F->getName()
                                                   << " already exists in library "
                                                   << funcToLib[F] << "\n");
            }
            else {
              funcToLib[F] = nameStr;
            }
          }
          else {
            SDEBUG("soaap.util.debug", 3, dbgs() << "DISubprogram \"" << func->getName() << "\" has no Function*\n");
          }
        }
      }
    }
  }
  cachingDone = true;
}

string DebugUtils::getEnclosingLibrary(Instruction* I) {
  return getEnclosingLibrary(I->getParent()->getParent());
}

string DebugUtils::getEnclosingLibrary(Function* F) {
  SDEBUG("soaap.util.debug", 3, dbgs() << "Finding enclosing library for inst in func " << F->getName() << "\n");
  if (!cachingDone) {
    cacheLibraryMetadata(F->getParent());
  }
  if (funcToLib.find(F) == funcToLib.end()) {
    SDEBUG("soaap.util.debug", 3, dbgs() << "Didn't find library for function " << F->getName() << "\n");
    return "";
  }
  else {
    return funcToLib[F];
  }
}

pair<string,int> DebugUtils::findGlobalDeclaration(GlobalVariable* G) {
  Module* M = G->getParent();
  if (NamedMDNode *NMD = M->getNamedMetadata("llvm.dbg.cu")) {
    for (int i=0; i<NMD->getNumOperands(); i++) {
      DICompileUnit* CU = cast<DICompileUnit>(NMD->getOperand(i));
      DIGlobalVariableArray globals = CU->getGlobalVariables();
      for (int j=0; j<globals.size(); j++) {
        DIGlobalVariable* GV = globals[j];
        if (GV->getVariable() == G) {
          return make_pair<string,int>(GV->getFilename().str(), GV->getLine());
        }
      }
    }
  }
  return make_pair<string,int>("",-1); 
}

tuple<string,int,string> DebugUtils::getInstLocation(Instruction* I) {
  if (DILocation* loc = dyn_cast_or_null<DILocation>(I->getMetadata("dbg"))) {
    Function* enclosingFunc = I->getParent()->getParent();
    string library = getEnclosingLibrary(enclosingFunc);
    return make_tuple(loc->getFilename(), loc->getLine(), library);
  }
  return make_tuple("",-1,"");
}

