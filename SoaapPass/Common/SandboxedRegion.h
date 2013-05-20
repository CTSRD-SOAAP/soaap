#ifndef SOAAP_COMMON_SANDBOXEDREGION_H
#define SOAAP_COMMON_SANDBOXEDREGION_H

#include "Common/Typedefs.h"

using namespace std;

namespace soaap {
  class Sandbox {
    public:
      Sandbox(InstructionVector pre, InstructionVector i, string n, bool p) : 
        instructions(i), name(n), persistent(p) { }
    
    private:
      string name;
      InstructionVector instructions;
      bool persistent;
  };
}

#endif
