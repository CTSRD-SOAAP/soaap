#ifndef SOAAP_COMMON_DEBUG_H
#define SOAAP_COMMON_DEBUG_H

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>

#ifdef NDEBUG
#define SDEBUG(X)
#else
#define SDEBUG(X)  \
  if (debugging()) { \
    do { X; } while (0); \
  }
#endif

using namespace llvm;

namespace soaap {
#ifndef NDEBUG  
  bool debugging();
  bool debugging(StringRef name, int verbosity);
#endif
  raw_ostream& debugs(StringRef DebugModuleName = "soaap", int VerbosityLevel = 0);
}

#endif
