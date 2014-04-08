#ifndef SOAAP_COMMON_DEBUG_H
#define SOAAP_COMMON_DEBUG_H

#include "Util/DebugUtils.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Debug.h"

#ifdef NDEBUG
#define SDEBUG(NAME,VERBOSITY,X)
#else
#define SDEBUG(NAME,VERBOSITY,X)  \
  if (debugging(NAME,VERBOSITY,__FUNCTION__)) { \
    do { \
         errs().changeColor(raw_ostream::Colors::GREEN); \
         errs() << "[" << __FUNCTION__ << " (" << NAME << ")]\n"; \
         errs().resetColor(); \
         X; \
       } while (0); \
  }
#endif

using namespace llvm;

namespace soaap {
#ifndef NDEBUG  
  bool debugging(StringRef ModuleName, int Verbosity, StringRef FunctionName);
  bool matches(StringRef name, StringRef pattern);
#endif
}

#endif
