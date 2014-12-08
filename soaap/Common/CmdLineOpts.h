#ifndef SOAAP_CMDLINEOPTS_H
#define SOAAP_CMDLINEOPTS_H

#include "llvm/Support/CommandLine.h"

#include <list>

using namespace llvm;
using namespace std;

namespace soaap {
  enum SandboxPlatformName {
    None, Annotated, Capsicum, Seccomp
  };
  enum ReportOutputFormat {
    Console, JSON
  };
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
      static bool Pedantic;
      static string DebugModule;
      static string DebugFunction;
      static int DebugVerbosity;
      static int SummariseTraces;
      static bool DumpRPCGraph;
      static SandboxPlatformName SandboxPlatform;
      static bool DumpDOTCallGraph;
      static list<ReportOutputFormat> ReportOutputFormats;
      static string ReportFilePrefix;
  };
}

#endif
