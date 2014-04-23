#ifndef SOAAP_OS_SYSCALLPROVIDER_H
#define SOAAP_OS_SYSCALLPROVIDER_H

#include <map>
#include <set>
#include <string>

using namespace std;

namespace soaap {
  class SysCallProvider {
    public:
      virtual bool isSysCall(string sysCall);
      virtual int getIdx(string sysCall);
      virtual string getSysCall(int idx);
      virtual void addSysCall(string sysCall); 
      virtual void initSysCalls() = 0;
    
    protected:
      set<string> sysCalls;
      map<string,int> sysCallToIdx;
      map<int,string> idxToSysCall;
  };
}

#endif
