#ifndef SOAAP_UTIL_TYPEUTILS_H
#define SOAAP_UTIL_TYPEUTILS_H

#include "Common/Typedefs.h"

#include <functional>

namespace soaap {
  class TypeUtils {
    public:
      static BitVector convertFunctionSetToBitVector(FunctionSet& set, function<int (Function*)> funcToIdMapper);
  };
}

#endif
