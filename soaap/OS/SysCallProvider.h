#ifndef SOAAP_OS_SYSCALLPROVIDER_H
#define SOAAP_OS_SYSCALLPROVIDER_H

#include <map>
#include <unordered_set>
#include <string>

using namespace std;

namespace soaap {
  class SysCallProvider {
    public:
      virtual bool isSysCall(string sysCall);
      virtual int getIdx(string sysCall);
      virtual string getSysCall(int idx);
      virtual void addSysCall(string sysCall, bool hasFdArg = false, int fdArgIdx = 0); 
      virtual bool hasFdArg(string sysCall);
      virtual int getFdArgIdx(string sysCall);
      virtual void initSysCalls() = 0;
    
    protected:
      unordered_set<string> sysCalls;
      map<string,int> sysCallToIdx;
      map<int,string> idxToSysCall;
      map<string,int> sysCallToFdArgIdx;
  };
}

#endif
