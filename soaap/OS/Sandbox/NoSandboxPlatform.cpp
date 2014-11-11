#include "OS/Sandbox/NoSandboxPlatform.h"

using namespace soaap;

bool NoSandboxPlatform::isSysCallPermitted(string name) {
  return true;
}

bool NoSandboxPlatform::doesSysCallRequireFDRights(string name) {
  return false;
}

bool NoSandboxPlatform::doesProvideProtection() {
  return false;
}
