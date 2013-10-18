#ifndef SOAAP_CLASSDEBUGINFOPASS_H
#define SOAAP_CLASSDEBUGINFOPASS_H

#include "llvm/Pass.h"

#define SOAAP_VTABLE_VAR_MDNODE_KIND "soaap_vtable_var"

using namespace llvm;
using namespace std;

namespace soaap {
  struct ClassDebugInfoPass : public ModulePass {
    static char ID;
    
    ClassDebugInfoPass() : ModulePass(ID) { }
    
    virtual bool runOnModule(Module& M);
  };
}

#endif
