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

void SysCallProvider::addSysCall(string sysCall, bool hasFdArg, int fdArgIdx) {
  static int nextIdx = 0;
  sysCalls.insert(sysCall);
  sysCallToIdx[sysCall] = nextIdx;
  idxToSysCall[nextIdx] = sysCall;
  if (hasFdArg) {
    sysCallToFdArgIdx[sysCall] = fdArgIdx;
  }
  nextIdx++;
}

bool SysCallProvider::hasFdArg(string sysCall) {
  return sysCallToFdArgIdx.find(sysCall) != sysCallToFdArgIdx.end();
}

int SysCallProvider::getFdArgIdx(string sysCall) {
  return sysCallToFdArgIdx[sysCall];
}
