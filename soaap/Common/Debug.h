#ifndef SOAAP_COMMON_DEBUG_H
#define SOAAP_COMMON_DEBUG_H

#include "Util/DebugUtils.h"

#include "llvm/Support/Debug.h"

#ifdef NDEBUG
#define SDEBUG(NAME,VERBOSITY,X)
#else
#define SDEBUG(NAME,VERBOSITY,X)  \
  if (debugging(NAME,VERBOSITY,__FUNCTION__)) { \
    do { \
         showPreamble(NAME, __FUNCTION__); \
         X; \
       } while (0); \
  }
#endif

using namespace llvm;
using namespace std;

namespace soaap {
#ifndef NDEBUG  
  bool debugging(string ModuleName, int Verbosity, string FunctionName);
  void showPreamble(string ModuleName, string FunctionName);
  bool matches(string name, string pattern);
#endif
}

#endif
