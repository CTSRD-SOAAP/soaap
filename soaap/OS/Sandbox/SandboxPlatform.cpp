#include "OS/Sandbox/SandboxPlatform.h"

using namespace soaap;

bool SandboxPlatform::isSysCallPermitted(string name) {
  return permittedSysCalls.count(name) != 0;
}

bool SandboxPlatform::doesSysCallRequireFDRights(string name) {
  return sysCallsReqFDRights.count(name) != 0;
}

void SandboxPlatform::addPermittedSysCall(string name, bool reqFDRights) {
  permittedSysCalls.insert(name);
  if (reqFDRights) {
    sysCallsReqFDRights.insert(name);
  }
}
