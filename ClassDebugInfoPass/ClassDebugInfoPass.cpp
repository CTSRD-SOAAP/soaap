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

                        SmallVector<string,2> scopes = getEnclosingScopes(varClassTypeDbg);
                        scopes.push_back("_GLOBAL__N_1"); // add the anonymous scope
                        string className = varClass.getName(); 
                        int templateBracket = className.find("<");
                        if (templateBracket != string::npos) {
                          className = className.erase(templateBracket);
                        }
                        scopes.push_back(className); // add the class name
                        
                        map<string,int> prefixToId;
                        int id = -1;
                        stringstream ss;
                        ss << "_ZTVN";
                        ss << mangleScopes(scopes, prefixToId, id);

                        // construct mangled name for vtable. Note we have to cater for
                        // compression whereby encoded prefixes that occur again are 
                        // replaced with S<seq-id>_ whereby seq-id is the prefix number
                        // starting from 0. S_ is a special case for the very first 
                        // substitution.
                        // class name may have template params in it
                        if (templateBracket != string::npos) { // encode template args
                          ss << "I"; // start of template args
                          DIArray templateParams = varClass.getTemplateParams();
                          for (int i=0; i<templateParams.getNumElements(); i++) {
                            DITemplateTypeParameter ttype(templateParams.getElement(i));
                            DICompositeType ctype(ttype.getType());
                            SmallVector<string,2> templateEnclosingScopes = getEnclosingScopes(ctype);
                            templateEnclosingScopes.push_back(ctype.getName());
                            if (templateEnclosingScopes.size() > 1) {
                              ss << "N"; // start of non-local qualified name
                            }
                            ss << mangleScopes(templateEnclosingScopes, prefixToId, id);
                            if (templateEnclosingScopes.size() > 1) {
                              ss << "E"; // end of non-local qualified name
                            }
                          }
                          ss << "E"; // end of template args and mangled identifier
                        }
                        ss << "E";
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

SmallVector<string,2> ClassDebugInfoPass::getEnclosingScopes(DIType& type) {
  SmallVector<string,2> enclosingScopes;
  DIScope scope = type.getContext();
  while (scope.isNameSpace() || scope.getTag() == dwarf::DW_TAG_class_type) {
    string name;
    if (scope.isNameSpace()) {
      DINameSpace ns(scope);
      name = ns.getName();
    }
    else {
      DICompositeType clazz(scope);
      name = clazz.getName();
    }
    if (name != "") {
      enclosingScopes.insert(enclosingScopes.begin(), name);
    }
    else {
      // break?
    }
    scope = scope.getContext();
  }
  return enclosingScopes;
}

string ClassDebugInfoPass::mangleScopes(SmallVector<string,2>& scopes, map<string,int>& prefixToId, int& nextPrefixId) {
  string mangled = "";
  string prefix = "";
  for (string s : scopes) {
    stringstream ss;
    if (prefixToId.find(s) == prefixToId.end()) {
      prefix += s;
      prefixToId[prefix] = nextPrefixId++;
      ss << s.length() << s;
    }
    else {
      int prefixId = prefixToId[s];
      ss << "S"; 
      if (prefixId > -1) {
        ss << prefixId;
      }
      ss << "_";
      prefix = ""; // reset prefix
    }
    mangled += ss.str();
  }
  return mangled;
}

char ClassDebugInfoPass::ID = 0;
static RegisterPass<ClassDebugInfoPass> X("soaap-insert-class-debug-info", "SOAAP Class Debug Info Pass", false, false);

static void addPasses(const PassManagerBuilder &Builder, PassManagerBase &PM) {
  PM.add(new ClassDebugInfoPass);
}

RegisterStandardPasses S(PassManagerBuilder::EP_EnabledOnOptLevel0, addPasses);
