#ifndef SOAAP_OS_SANDBOX_SECCOMPBPF_H
#define SOAAP_OS_SANDBOX_SECCOMPBPF_H

#include "OS/Sandbox/SandboxPlatform.h"

namespace soaap {
  class SeccompBPF : public SandboxPlatform {
    public:
      SeccompBPF();
  };
}

#endif
