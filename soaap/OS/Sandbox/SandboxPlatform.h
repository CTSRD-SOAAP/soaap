#ifndef SOAAP_OS_SANDBOX_SANDBOXPLATFORM_H
#define SOAAP_OS_SANDBOX_SANDBOXPLATFORM_H

#include <string>
#include <unordered_set>

using namespace std;

namespace soaap {

  class SandboxPlatform {
    public:
      // Sandbox is allowed to perform system call "name". If it takes a 
      // file descriptor argument, then it may require additional rights
      // to be able to complete. This can be determined by calling
      // doesSysCallRequireFDRights(name).
      virtual bool isSysCallPermitted(string name);

      // Sandbox requires rights permitting it to perform system call "name"
      // on its file descriptor argument. The return value only makes sense
      // if isSysCallPermitted(name) returns true.
      virtual bool doesSysCallRequireFDRights(string name);

    protected:
      SandboxPlatform() { } // this is an abstract class
      unordered_set<string> permittedSysCalls;
      unordered_set<string> sysCallsReqFDRights;

      void addPermittedSysCall(string name, bool reqFDRights = false);
  };
}

#endif
