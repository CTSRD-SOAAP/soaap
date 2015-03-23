#ifndef SOAAP_CMDLINEOPTS_H
#define SOAAP_CMDLINEOPTS_H

#include "llvm/Support/CommandLine.h"

#include <list>

using namespace llvm;
using namespace std;

namespace soaap {
  enum class SandboxPlatformName {
    None, Annotated, Capsicum, Seccomp
  };
  enum class ReportOutputFormat {
    Text, HTML, JSON, XML
  };
  enum class SoaapMode {
    Null, Vuln, Correct, InfoFlow, Custom, All
  };
  enum class SoaapAnalysis {
    None, Vuln, Globals, SysCalls, PrivCalls, SandboxedFuncs, InfoFlow, All
  };
  class CmdLineOpts {
    public:
      static bool EmPerf;
      static list<string> VulnerableVendors;
      static list<string> VulnerableModules;
      static bool ContextInsens;
      static bool ListSandboxedFuncs;
      static bool ListPrivilegedFuncs;
      static bool ListFPCalls;
      static bool InferFPTargets;
      static bool ListFPTargets;
      static bool ListAllFuncs;
      static bool SkipGlobalVariableAnalysis;
      static bool Pedantic;
      static string DebugModule;
      static string DebugFunction;
      static int DebugVerbosity;
      static int SummariseTraces;
      static bool DumpRPCGraph;
      static SandboxPlatformName SandboxPlatform;
      static bool DumpDOTCallGraph;
      static bool PrintCallGraph;
      static list<ReportOutputFormat> ReportOutputFormats;
      static string ReportFilePrefix;
      static bool SysCallTraces;
      static cl::OptionCategory SoaapCategory;
      static SoaapMode Mode;
      static list<SoaapAnalysis> OutputTraces;
      static list<SoaapAnalysis> SoaapAnalyses;
      static list<string> NoWarnModules;
      static list<string> WarnModules;
  
      template<typename T>
      static bool isSelected(T opt, list<T> optsList) {
        return find(optsList.begin(), optsList.end(), opt) != optsList.end();
      }
  };

}

#endif
