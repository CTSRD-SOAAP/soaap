#include "Util/DebugUtils.h"

#include "Common/Debug.h"

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"

using namespace soaap;

map<DICompileUnit, DILLVMModule> DebugUtils::cuToMod;
map<Function*, DICompileUnit> DebugUtils::funcToCU;
bool DebugUtils::cachingDone = false;

void DebugUtils::cacheDebugMetadata(Module* M) {
  if (NamedMDNode* N = M->getNamedMetadata("llvm.dbg.cu")) {
    SDEBUG("soaap.util.debug", 3, dbgs() << "Found llvm.dbg.cu metadata, " << N->getNumOperands() << " operands\n");
    for (int i=0; i<N->getNumOperands(); i++) {
      DICompileUnit cu(N->getOperand(i));
      DIArray funcs = cu.getSubprograms();
      SDEBUG("soaap.util.debug", 3, dbgs() << "Compile unit " << i << " has " << funcs.getNumElements() << " funcs\n");
      for (int j=0; j<funcs.getNumElements(); j++) {
        DISubprogram func = (DISubprogram)funcs.getElement(j);
        SDEBUG("soaap.util.debug", 3, dbgs() << INDENT_1 << "Func: " << func.getName() << "\n");
        funcToCU[func.getFunction()] = cu;
      }
    }
  }
  if (NamedMDNode* N = M->getLLVMModuleMetadata()) {
    SDEBUG("soaap.util.debug", 3, dbgs() << "Found llvm.module metadata\n");
    DILLVMModule Mod(N->getOperand(0));
    cacheCompileUnitToModule(Mod);
  }
  cachingDone = true;
}

void DebugUtils::cacheCompileUnitToModule(DILLVMModule Mod) {
  SDEBUG("soaap.util.debug", 3, dbgs() << "Processing module " << Mod.getName() << "\n");
  DIArray cus = Mod.getCUs();
  SDEBUG("soaap.util.debug", 3, dbgs() << INDENT_1 << "Num of CUs: " << cus.getNumElements() << "\n");
  for (unsigned i = 0; i < cus.getNumElements(); i++) {
    DICompileUnit cu = static_cast<DICompileUnit>(cus.getElement(i));
    SDEBUG("soaap.util.debug", 3, dbgs() << "storing mapping from cu " << cu.getSplitDebugFilename() << "to " << Mod.getName() << "\n");
    cuToMod[cu] = Mod;
  }
  DIArray modules = Mod.getModules();
  SDEBUG("soaap.util.debug", 3, dbgs() << INDENT_1 << "Num of submods: " << modules.getNumElements() << "\n");
  for (unsigned i = 0; i < modules.getNumElements(); i++) {
    DILLVMModule submod = static_cast<DILLVMModule>(modules.getElement(i));
    SDEBUG("soaap.util.debug", 3, dbgs() << INDENT_1 << "Recursing on " << submod.getName() << "\n");
    cacheCompileUnitToModule(submod);
  }
}

string DebugUtils::getEnclosingModule(Instruction* I) {
  Function* F = I->getParent()->getParent();
  SDEBUG("soaap.util.debug", 3, dbgs() << "Finding enclosing module for inst in func " << F->getName() << "\n");
  if (!cachingDone) {
    cacheDebugMetadata(F->getParent());
  }
  if (funcToCU.find(F) != funcToCU.end()) {
    DICompileUnit cu = funcToCU[F];
    SDEBUG("soaap.util.debug", 3, dbgs() << "Found compile unit for " << F->getName() << "\n");
    if (cuToMod.find(cu) != cuToMod.end()) {
      DILLVMModule mod = cuToMod[cu];
      SDEBUG("soaap.util.debug", 3, dbgs() << "Found module mapped to: " << mod.getName() << "\n");
      return mod.getName();
    }
    SDEBUG("soaap.util.debug", 3, dbgs() << "Didn't find module for compile unit\n");
  }
  return "";
}
