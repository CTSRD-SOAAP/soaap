#ifndef SOAAP_UTILS_SANDBOXUTILS_H
#define SOAAP_UTILS_SANDBOXUTILS_H

#include <string>
#include <map>

using namespace std;

namespace soaap {
  class SandboxUtils {
    public:
      static void assignBitIdxToSandboxName(string sandboxName);
      static int getBitIdxFromSandboxName(string sandboxName);
      static string stringifySandboxNames(int sandboxNames);
    
    private:
      static map<string,int> sandboxNameToBitIdx;
      static map<int,string> bitIdxToSandboxName;
      static int nextSandboxNameBitIdx;
  };
}

#endif
