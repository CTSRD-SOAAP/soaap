#include "Debug.h"

#include <Common/CmdLineOpts.h>

#include <llvm/Support/raw_ostream.h>

#include <fnmatch.h>
#include <stdlib.h>
#include <unistd.h>

using namespace llvm;
using namespace soaap;

#ifndef NDEBUG
bool soaap::debugging(StringRef ModuleName, int Verbosity, StringRef FunctionName) {
  if (CmdLineOpts::DebugModule.empty() || Verbosity > CmdLineOpts::DebugVerbosity) {
    return false;
  }
  return matches(ModuleName, CmdLineOpts::DebugModule)
      && (CmdLineOpts::DebugFunction.empty() || matches(FunctionName, CmdLineOpts::DebugFunction));
}

bool soaap::matches(StringRef pattern, StringRef name) {
  // Let e.g. 'soaap.infoflow' match 'soaap.infoflow.anything'.
  if (name.size() > pattern.size()
      && name.startswith(pattern)
      && name[pattern.size()] == '.') {
    return true;
  }

  // Use fnmatch()'s normal wildcard expansion.
  return (fnmatch(pattern.str().c_str(), name.str().c_str(), 0) == 0);
}
#endif
