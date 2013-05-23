#ifndef SOAAP_COMMON_SANDBOX_H
#define SOAAP_COMMON_SANDBOX_H

#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {
  class Sandbox {
    public:
      Sandbox(string n, int i, Function* entry, bool p) 
          : name(n), nameIdx(i), entryPoint(entry), persistent(p) { }
      int getNameIdx();
      Function* getEntryPoint();

    private:
      string name;
      int nameIdx;
      int clearances;
      FunctionVector callgates;
      Function* entryPoint;
      bool persistent;
  };
  typedef SmallVector<Sandbox*,16> SandboxVector;
}

#endif
