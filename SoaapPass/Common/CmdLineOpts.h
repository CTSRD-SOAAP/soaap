#ifndef SOAAP_CMDLINEOPTS_H
#define SOAAP_CMDLINEOPTS_H

#include "llvm/Support/CommandLine.h"

#include <list>

using namespace llvm;
using namespace std;

namespace soaap {
  class CmdLineOpts {
    public:
      static bool EmPerf;
      static list<string> VulnerableVendors;
      static bool ContextInsens;
      static bool ListSandboxedFuncs;
      static bool ListFPCalls;
      static bool InferFPTargets;
      static bool ListFPTargets;
      static bool ListAllFuncs;
      static string DumpVirtualCallees;
      static string ReadVirtualCallees;
  };
}

#endif