#ifndef SOAAP_UTIL_DEBUGUTILS_H
#define SOAAP_UTIL_DEBUGUTILS_H

#define INDENT_1 "  "
#define INDENT_2 "    "
#define INDENT_3 "      "
#define INDENT_4 "        "
#define INDENT_5 "          "
#define INDENT_6 "            "

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

#include <map>
#include <string>

using namespace llvm;
using namespace std;

namespace soaap {
  class DebugUtils {
    public:
      static string getEnclosingModule(Instruction* I);

    protected:
      static bool cachingDone;
      static map<DICompileUnit, DILLVMModule> cuToMod;
      static map<Function*, DICompileUnit> funcToCU;
      static void cacheDebugMetadata(Module* M);
      static void cacheCompileUnitToModule(DILLVMModule Mod);
  };
}

#endif
