#ifndef SOAAP_OS_LINUXSYSCALLPROVIDER_H
#define SOAAP_OS_LINUXSYSCALLPROVIDER_H

#include "OS/SysCallProvider.h"

namespace soaap {
  class LinuxSysCallProvider : public SysCallProvider {
    public:
      virtual void initSysCalls();
  };
}

#endif
