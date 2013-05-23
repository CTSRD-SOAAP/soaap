#include "Common/Sandbox.h"

using namespace soaap;

Function* Sandbox::getEntryPoint() {
  return entryPoint;
}

int Sandbox::getNameIdx() {
  return nameIdx;
}
