#include "Util/DebugUtils.h"

#include "Common/Debug.h"

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"

using namespace soaap;

map<Function*, pair<string,int> > DebugUtils::funcToDebugMetadata;
bool DebugUtils::cachingDone = false;

void DebugUtils::cacheFuncToDebugMetadata(Module* M) {
  if (NamedMDNode* N = M->getNamedMetadata("llvm.dbg.cu")) {
    SDEBUG("soaap.util.debug", 3, dbgs() << "Found llvm.dbg.cu metadata, " << N->getNumOperands() << " operands\n");
    for (int i=0; i<N->getNumOperands(); i++) {
      DICompileUnit cu(N->getOperand(i));
      DIArray funcs = cu.getSubprograms();
      SDEBUG("soaap.util.debug", 3, dbgs() << "Compile unit " << i << " has " << funcs.getNumElements() << " funcs\n");
      for (int j=0; j<funcs.getNumElements(); j++) {
        DISubprogram func = (DISubprogram)funcs.getElement(j);
        SDEBUG("soaap.util.debug", 3, dbgs() << INDENT_1 << "Func: " << func.getName() << "\n");
        funcToDebugMetadata[func.getFunction()] = make_pair<string,int>(func.getFilename(), func.getLineNumber());
      }
    }
  }
  cachingDone = true;
}

pair<string,int> DebugUtils::getFunctionLocation(Function* F) {
  if (!cachingDone) {
    SDEBUG("soaap.util.debug", 3, dbgs() << "caching function locations\n");
    cacheFuncToDebugMetadata(F->getParent());
  }
  if (funcToDebugMetadata.find(F) == funcToDebugMetadata.end()) {
    return make_pair<string,int>("",-1);
  }
  else {
    return funcToDebugMetadata[F];
  }
}
