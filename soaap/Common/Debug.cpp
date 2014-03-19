#include "Debug.h"

#include <Common/CmdLineOpts.h>

#include <llvm/Support/raw_ostream.h>

#include <fnmatch.h>
#include <stdlib.h>
#include <unistd.h>

using namespace llvm;
using namespace soaap;

#ifndef NDEBUG
bool soaap::debugging(StringRef Name, int Verbosity) {
  if (CmdLineOpts::DebugModule.empty() || Verbosity > CmdLineOpts::DebugVerbosity) {
    return false;
  }
  // Let e.g. 'soaap.infoflow' match 'soaap.infoflow.anything'.
  string debugModule = CmdLineOpts::DebugModule;
  if (Name.size() > debugModule.length()
      and Name.startswith(debugModule)
      and Name[debugModule.length()] == '.')
    return true;

  // Use fnmatch()'s normal wildcard expansion.
  return (fnmatch(debugModule.c_str(), Name.str().c_str(), 0) == 0);
}
#endif
