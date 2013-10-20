#ifndef SOAAP_CLASSDEBUGINFOPASS_H
#define SOAAP_CLASSDEBUGINFOPASS_H

#include "llvm/DebugInfo.h"
#include "llvm/Pass.h"
#include "llvm/ADT/SmallVector.h"

#include <map>

#define SOAAP_VTABLE_VAR_MDNODE_KIND "soaap_vtable_var"
#define SOAAP_VTABLE_NAME_MDNODE_KIND "soaap_vtable_name"

using namespace llvm;
using namespace std;

namespace soaap {
  struct ClassDebugInfoPass : public ModulePass {
    public:
      static char ID;
      ClassDebugInfoPass() : ModulePass(ID) { }
      virtual bool runOnModule(Module& M);

    private:
      SmallVector<string,2> getEnclosingScopes(DIType& type);
      string mangleScopes(SmallVector<string,2>& scopes, map<string,int>& prefixToId, int& nextPrefixId);
  };
}

#endif
