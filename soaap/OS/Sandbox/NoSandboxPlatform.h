#ifndef SOAAP_OS_SANDBOX_NOSANDBOXPLATFORM_H
#define SOAAP_OS_SANDBOX_NOSANDBOXPLATFORM_H

#include "OS/Sandbox/SandboxPlatform.h"

namespace soaap {
  class NoSandboxPlatform : public SandboxPlatform {
    public:
      virtual bool isSysCallPermitted(string name);
      virtual bool doesSysCallRequireFDRights(string name);
      virtual bool doesProvideProtection();
  };
}

#endif
