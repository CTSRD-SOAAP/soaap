#ifndef SOAAP_COMMON_DEBUG_H
#define SOAAP_COMMON_DEBUG_H

#include "Common/CmdLineOpts.h"
#include "Util/DebugUtils.h"

#include "llvm/Support/Debug.h"

#ifdef NDEBUG
#define SDEBUG(NAME,VERBOSITY,X)
#else
#define SDEBUG(NAME,VERBOSITY,X)  \
  if (!CmdLineOpts::DebugModule.empty() && VERBOSITY <= CmdLineOpts::DebugVerbosity) {\
    if (soaap::matches(NAME, CmdLineOpts::DebugModule) \
        && (CmdLineOpts::DebugFunction.empty() || soaap::matches(__FUNCTION__, CmdLineOpts::DebugFunction))) {\
      do { \
        showPreamble(NAME, __FUNCTION__); \
        X; \
      } while (0); \
    }\
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
