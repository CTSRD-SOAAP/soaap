#include "ClassDebugInfoPass.h"

#include "llvm/DebugInfo.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"

#include <sstream>

#include "soaap.h"

using namespace llvm;
using namespace std;
using namespace soaap;

bool ClassDebugInfoPass::runOnModule(Module& M) {
  outs() << "* Running " << getPassName() << "\n";
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (!isa<IntrinsicInst>(&*I)) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          if (!isa<IntrinsicInst>(C) && C->getCalledFunction() == NULL) {
            if (Value* calledVal = C->getCalledValue()) {
              // obtain the vtable global var for the static type and insert reference to
              // it as metadata
              if (LoadInst* receiver = dyn_cast<LoadInst>(C->getArgOperand(0)->stripPointerCasts())) {
                Value* receiverVar = receiver->getPointerOperand();
                DEBUG(dbgs() << "receiverVar: " << *receiverVar << "\n");

                // To get the static type of the receiver var, look for the call to llvm.dbg.declare()
                // (While not difficult to do, llvm already provides a helper function to do this):
                if (DbgDeclareInst* dbgDecl = FindAllocaDbgDeclare(receiverVar)) {
                  DEBUG(dbgs() << "receiverVar dbgDecl: " << *dbgDecl << "\n");

                  DIVariable varDbg(dbgDecl->getVariable());
                  DIDerivedType varPtrTypeDbg(varDbg.getType());
                  DIDerivedType varClassTypeDbg(varPtrTypeDbg.getTypeDerivedFrom()); 
                  while (varClassTypeDbg.getTag() == dwarf::DW_TAG_typedef) { // walk over typedefs...
                    varClassTypeDbg = (DIDerivedType)varClassTypeDbg.getTypeDerivedFrom();
                  }

                  DICompositeType varClass(varClassTypeDbg);
                  string vtableGlobalName;
                  if (MDString* varClassId = varClass.getIdentifier()) {
                    string varClassIdStr = varClassId->getString().str();
                    DEBUG(dbgs() << "receiverVar class name: " << varClassIdStr << "\n");
                    // construct vtable global id by replacing _ZTS with _ZTV
                    vtableGlobalName = varClassIdStr.replace(0, 4, "_ZTV");
                  }
                  else {
                    // anonymous namespace class, so append class name to _ZTVN12_GLOBAL__N_1
                    // So a class Foo would be: _ZTVN12_GLOBAL__N_13FooE
                    string className = varClass.getName();
                    stringstream ss;
                    ss << className.length();
                    vtableGlobalName = "_ZTVN12_GLOBAL__N_1" + ss.str() + className + "E";
                  }
                  dbgs() << "Looking for vtable global " << vtableGlobalName << "\n";
                  if (GlobalVariable* vtableGlobal = M.getGlobalVariable(vtableGlobalName, true)) {
                    MDNode* md = MDNode::get(M.getContext(), vtableGlobal);
                    C->setMetadata(SOAAP_VTABLE_VAR_MDNODE_KIND, md);
                  }
                  else {
                    dbgs() << "ERROR: could not find vtable global " << vtableGlobalName << "\n";
                    return true;
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return true;
}

char ClassDebugInfoPass::ID = 0;
static RegisterPass<ClassDebugInfoPass> X("soaap-insert-class-debug-info", "SOAAP Class Debug Info Pass", false, false);

static void addPasses(const PassManagerBuilder &Builder, PassManagerBase &PM) {
  PM.add(new ClassDebugInfoPass);
}

RegisterStandardPasses S(PassManagerBuilder::EP_EnabledOnOptLevel0, addPasses);
