#include "Debug.h"

#include <Common/CmdLineOpts.h>

#include <llvm/Support/raw_ostream.h>

#include <regex>
#include <stdlib.h>
#include <unistd.h>

using namespace llvm;
using namespace soaap;

#ifndef NDEBUG
bool soaap::debugging(string ModuleName, int Verbosity, string FunctionName) {
  if (CmdLineOpts::DebugModule.empty() || Verbosity > CmdLineOpts::DebugVerbosity) {
    return false;
  }
  return matches(ModuleName, CmdLineOpts::DebugModule)
      && (CmdLineOpts::DebugFunction.empty() || matches(FunctionName, CmdLineOpts::DebugFunction));
}

bool soaap::matches(string name, string pattern) {
  regex reg(pattern);
  return regex_match(name, reg);
}
#endif
