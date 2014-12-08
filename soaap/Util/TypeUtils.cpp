
#include "Util/TypeUtils.h"

#include "llvm/Support/Debug.h"

using namespace soaap;

BitVector TypeUtils::convertFunctionSetToBitVector(FunctionSet& set, function<int (Function*)> funcToIdMapper) {
  BitVector vector;
  for (Function* F : set) {
    int idx = funcToIdMapper(F);
    if (vector.size() <= idx) {
      vector.resize(idx+1);
    }
    vector.set(idx);
  }
  return vector;
}

string TypeUtils::stringifyStringSet(StringSet& strings) {
  string str = "[";
  bool first = true;
  for (string s : strings) {
    if (!first) 
      str += ",";
    str += s;
    first = false;
  }
  str += "]";
  return str;
}
