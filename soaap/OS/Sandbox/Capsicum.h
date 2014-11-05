#ifndef SOAAP_OS_SANDBOX_CAPSICUM_H
#define SOAAP_OS_SANDBOX_CAPSICUM_H

#include "OS/Sandbox/SandboxPlatform.h"

namespace soaap {
  class Capsicum : public SandboxPlatform {
    public:
      Capsicum();
  };
}

#endif
