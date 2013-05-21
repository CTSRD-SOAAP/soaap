#include "Utils/SandboxUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;
using namespace llvm;

int SandboxUtils::nextSandboxNameBitIdx = 0;
map<string,int> SandboxUtils::sandboxNameToBitIdx;
map<int,string> SandboxUtils::bitIdxToSandboxName;

string SandboxUtils::stringifySandboxNames(int sandboxNames) {
  string sandboxNamesStr = "[";
  int currIdx = 0;
  bool first = true;
  for (currIdx=0; currIdx<=31; currIdx++) {
    if (sandboxNames & (1 << currIdx)) {
      string sandboxName = bitIdxToSandboxName[currIdx];
      if (!first) 
        sandboxNamesStr += ",";
      sandboxNamesStr += sandboxName;
      first = false;
    }
  }
  sandboxNamesStr += "]";
  return sandboxNamesStr;
}

void SandboxUtils::assignBitIdxToSandboxName(string sandboxName) {
  if (sandboxNameToBitIdx.find(sandboxName) == sandboxNameToBitIdx.end()) {
    dbgs() << "    Assigning index " << nextSandboxNameBitIdx << " to sandbox name \"" << sandboxName << "\"\n";
    sandboxNameToBitIdx[sandboxName] = nextSandboxNameBitIdx;
    bitIdxToSandboxName[nextSandboxNameBitIdx] = sandboxName;
    nextSandboxNameBitIdx++;
  }
}

int SandboxUtils::getBitIdxFromSandboxName(string sandboxName) {
  return sandboxNameToBitIdx[sandboxName];
}
