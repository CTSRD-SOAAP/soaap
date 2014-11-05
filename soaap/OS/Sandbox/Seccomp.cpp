#include "OS/Sandbox/Seccomp.h"

using namespace soaap;

Seccomp::Seccomp() {
  addPermittedSysCall("sigreturn");
  addPermittedSysCall("exit");
  addPermittedSysCall("read", true);
  addPermittedSysCall("write", true);
}
