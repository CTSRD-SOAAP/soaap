#ifndef SOAAP_OS_FREEBSDSYSCALLPROVIDER_H
#define SOAAP_OS_FREEBSDSYSCALLPROVIDER_H

#include "OS/SysCallProvider.h"

namespace soaap {
  class FreeBSDSysCallProvider : public SysCallProvider {
    public:
      virtual void initSysCalls();
  };
}

#endif
