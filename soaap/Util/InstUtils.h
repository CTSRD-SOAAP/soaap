#ifndef SOAAP_UTIL_INSTUTILS_H
#define SOAAP_UTIL_INSTUTILS_H

#include "llvm/IR/Instructions.h"

using namespace llvm;

namespace soaap {

  class InstUtils {
    public:
      static void EmitInstLocation(Instruction* I);
  };
}

#endif
