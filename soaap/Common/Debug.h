#ifndef SOAAP_COMMON_DEBUG_H
#define SOAAP_COMMON_DEBUG_H

#include <llvm/ADT/StringRef.h>

#ifdef NDEBUG
#define SDEBUG(NAME,VERBOSITY,X)
#else
#define SDEBUG(NAME,VERBOSITY,X)  \
  if (debugging(NAME,VERBOSITY)) { \
    do { X; } while (0); \
  }
#endif

using namespace llvm;

namespace soaap {
#ifndef NDEBUG  
  bool debugging(StringRef name, int verbosity);
#endif
}

#endif
