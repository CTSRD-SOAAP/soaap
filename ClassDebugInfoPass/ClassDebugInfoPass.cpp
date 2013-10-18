#include "ClassDebugInfoPass.h"

#include "llvm/DebugInfo.h"
#include "llvm/PassManager.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
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
          if (!isa<IntrinsicInst>(C) && C->getCalledFunction() == NULL && !C->isInlineAsm()) {
            if (Value* calledVal = C->getCalledValue()) {
              if (GlobalAlias* alias = dyn_cast<GlobalAlias>(calledVal)) {
                if (alias->resolveAliasedGlobal(false) == NULL) {
                  calledVal->dump();
                  dbgs() << "ERROR: called value is an alias, but the aliasing chain contains a cycle!\n";
                }
              }
              else {
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
                    while (varClassTypeDbg.isDerivedType() && !varClassTypeDbg.isCompositeType()) { // walk over typedefs, const types, etc.
                      varClassTypeDbg = (DIDerivedType)varClassTypeDbg.getTypeDerivedFrom();
                    }

                    if (varClassTypeDbg.getTag() == dwarf::DW_TAG_class_type || varClassTypeDbg.getTag() == dwarf::DW_TAG_structure_type) {
                      DICompositeType varClass(varClassTypeDbg);
                      string vtableGlobalName;
                      if (MDString* varClassId = varClass.getIdentifier()) {
                        // class is not inside an anonymous namespace, so its id will be
                        // global (i.e. non-internal linkage) and so we can just store 
                        // the name of the vtable global (and actually, the vtable global
                        // may not be in this compilation unit).
                        string varClassIdStr = varClassId->getString().str();
                        DEBUG(dbgs() << "receiverVar class name: " << varClassIdStr << "\n");
                        // construct vtable global id by replacing _ZTS with _ZTV
                        vtableGlobalName = varClassIdStr.replace(0, 4, "_ZTV");
                        Constant* vtableGlobalNameConstant = ConstantDataArray::getString(M.getContext(), vtableGlobalName, false);
                        MDNode* md = MDNode::get(M.getContext(), vtableGlobalNameConstant);
                        C->setMetadata(SOAAP_VTABLE_NAME_MDNODE_KIND, md);
                      }
                      else {
                        // anonymous namespace class, so append class name to _ZTVN12_GLOBAL__N_1
                        // So a class Foo would be: _ZTVN12_GLOBAL__N_13FooE. Note that as this is
                        // an anonymous namespace class, the vtable global var will be defined in
                        // this compilation unit. This var will have internal linkage incase there
                        // are other classes with the same name also within anonymous namespaces.
                        // Hence, we store a ref to the global in the MDNode so that when we 
                        // access it post-linking, it will be renamed automatically by LLVM's linker.

                        // The anonymousname space may have enclosing namespaces, so we'll need
                        // to know what they are in order to construct the vtable global name
                        // correctly.

                        SmallVector<string,2> enclosingNameSpaces;
                        DIScope ns = varClassTypeDbg.getContext();
                        while (ns.isNameSpace()) {
                          DINameSpace nsDbg(ns);
                          string nsName = nsDbg.getName();
                          if (nsName != "") {
                            enclosingNameSpaces.insert(enclosingNameSpaces.begin(), nsName);
                          }
                          else {
                            // break?
                          }
                          ns = nsDbg.getContext();
                        }

                        // construct mangled name for vtable
                        stringstream ss;
                        ss << "_ZTVN";
                        for (string s : enclosingNameSpaces) {
                          ss << s.length() << s;
                        }
                        ss << "12_GLOBAL__N_1";
                        string className = varClass.getName();
                        ss << className.length() << className << "E";
                        vtableGlobalName = ss.str();
                        dbgs() << "Looking for vtable global " << vtableGlobalName << "\n";
                        if (GlobalVariable* vtableGlobal = M.getGlobalVariable(vtableGlobalName, true)) {
                          MDNode* md = MDNode::get(M.getContext(), vtableGlobal);
                          C->setMetadata(SOAAP_VTABLE_VAR_MDNODE_KIND, md);
                        }
                        else {
                          dbgs() << "ERROR: could not find vtable global " << vtableGlobalName << "\n";
                          C->dump();
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
