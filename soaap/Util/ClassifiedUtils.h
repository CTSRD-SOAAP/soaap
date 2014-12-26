#ifndef SOAAP_UTILS_CLASSIFIEDUTILS_H
#define SOAAP_UTILS_CLASSIFIEDUTILS_H

#include "Common/Typedefs.h"

#include <string>
#include <map>

using namespace std;

namespace soaap {
  class ClassifiedUtils {
    public:
      static void assignBitIdxToClassName(string className);
      static int getBitIdxFromClassName(string className);
      static string stringifyClassNames(int classNames);
      static StringVector convertNamesToVector(int classNames);
    
    private:
      static map<string,int> classNameToBitIdx;
      static map<int,string> bitIdxToClassName;
      static int nextClassNameBitIdx;
  };
}

#endif
