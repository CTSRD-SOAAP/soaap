#include "Debug.h"

#include <Common/CmdLineOpts.h>

#include <llvm/ADT/Twine.h>
#include <llvm/Support/raw_ostream.h>

#include <fnmatch.h>
#include <stdlib.h>
#include <unistd.h>

using namespace llvm;
using namespace soaap;

#ifndef NDEBUG
bool soaap::debugging() {
  return !CmdLineOpts::DebugModule.empty();
}

bool soaap::debugging(StringRef Name, int Verbosity) {
  if (!debugging() || Verbosity > CmdLineOpts::DebugVerbosity) {
    return false;
  }
  // Let e.g. 'soaap.infoflow' match 'soaap.infoflow.anything'.
  string debugModule = CmdLineOpts::Debug;
  if (Name.size() > debugModule.length()
      and Name.startswith(debugModule)
      and Name[debugModule.length()] == '.')
    return true;

  // Use fnmatch()'s normal wildcard expansion.
  return (fnmatch(debugModule.c_str(), Name.str().c_str(), 0) == 0);
}
#endif

raw_ostream& soaap::debugs(StringRef DebugModuleName, int VerbosityLevel) {
#ifndef NDEBUG
  if (debugging(DebugModuleName) && VerbosityLevel >= CmdLineOpts::DebugVerbosity) {
    static raw_ostream& ErrStream = llvm::errs();
    return ErrStream;
  }
#endif

  static raw_null_ostream NullStream;
  return NullStream;
}

#ifndef NDEBUG
#include <llvm/Support/Signals.h>

namespace {

class StaticDebugInit {
public:
  StaticDebugInit() {
    sys::PrintStackTraceOnErrorSignal();
  }

} DebugInit;

}
#endif
