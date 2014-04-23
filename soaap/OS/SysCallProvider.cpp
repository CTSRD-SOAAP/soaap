#include "OS/SysCallProvider.h"

using namespace soaap;

bool SysCallProvider::isSysCall(string sysCall) {
  return sysCalls.count(sysCall) != 0;
}

int SysCallProvider::getIdx(string sysCall) {
  return (sysCallToIdx.find(sysCall) != sysCallToIdx.end()) ? sysCallToIdx[sysCall] : -1;
}

string SysCallProvider::getSysCall(int idx) {
  return (idxToSysCall.find(idx) != idxToSysCall.end()) ? idxToSysCall[idx] : "";
}

void SysCallProvider::addSysCall(string sysCall) {
  static int nextIdx = 0;
  sysCalls.insert(sysCall);
  sysCallToIdx[sysCall] = nextIdx;
  idxToSysCall[nextIdx] = sysCall;
  nextIdx++;
}
