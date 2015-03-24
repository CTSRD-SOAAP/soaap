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
      static string getEnclosingLibrary(Instruction* I);
      static string getEnclosingLibrary(Function* F);

    protected:
      static bool cachingDone;
      static map<Function*, string> funcToLib;
      static void cacheLibraryMetadata(Module* M);

  };
}

#endif
