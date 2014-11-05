#ifndef SOAAP_OS_SANDBOX_SECCOMP_H
#define SOAAP_OS_SANDBOX_SECCOMP_H

#include "OS/Sandbox/SandboxPlatform.h"

namespace soaap {
  class Seccomp : public SandboxPlatform {
    public:
      Seccomp();
  };
}

#endif
