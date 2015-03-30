#include "Util/DebugUtils.h"

#include "Common/Debug.h"

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"

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
      MDNode* cus = cast<MDNode>(lib->getOperand(1).get());
      for (int j=0; j<cus->getNumOperands(); j++) {
        DICompileUnit cu(cast<MDNode>(cus->getOperand(j).get()));
        DIArray funcs = cu.getSubprograms();
        for (int k=0; k<funcs.getNumElements(); k++) {
          DISubprogram func = static_cast<DISubprogram>(funcs.getElement(k));
          Function* F = func.getFunction();
          SDEBUG("soaap.util.debug", 4, dbgs() << INDENT_1 << "Found func: " << F->getName() << "\n");
          if (funcToLib.find(F) != funcToLib.end()) {
            dbgs() << "WARNING: Function " << F->getName()
                   << " already exists in library "
                   << funcToLib[F] << "\n";
          }
          funcToLib[F] = nameStr;
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
