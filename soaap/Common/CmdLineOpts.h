/*
 * Copyright (c) 2013-2015 Khilan Gudka
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef SOAAP_CMDLINEOPTS_H
#define SOAAP_CMDLINEOPTS_H

#include "llvm/Support/CommandLine.h"

#include <list>

using namespace llvm;
using namespace std;

namespace soaap {
  enum class OperatingSystemName {
    FreeBSD, Linux
  };
  enum class SandboxPlatformName {
    None, Annotated, Capsicum, Chroot, Seccomp, SeccompBPF
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
      static bool GenSandboxes;
      static list<string> VulnerableVendors;
      static list<string> VulnerableLibs;
      static bool ContextInsens;
      static bool ListSandboxedFuncs;
      static bool ListPrivilegedFuncs;
      static bool ListFPCalls;
      static bool InferFPTargets;
      static bool ListFPTargets;
      static bool ListAllFuncs;
      static bool SkipGlobalVariableAnalysis;
      static bool Pedantic;
      static bool EmitLeakedRights;
      static string DebugModule;
      static string DebugFunction;
      static int DebugVerbosity;
      static int SummariseTraces;
      static bool DumpRPCGraph;
      static OperatingSystemName OperatingSystem;
      static SandboxPlatformName SandboxPlatform;
      static string SandboxPolicy;
      static bool DumpDOTCallGraph;
      static bool PrintCallGraph;
      static list<ReportOutputFormat> ReportOutputFormats;
      static string ReportFilePrefix;
      static bool PrettyPrint;
      static cl::OptionCategory SoaapCategory;
      static SoaapMode Mode;
      static list<SoaapAnalysis> OutputTraces;
      static list<SoaapAnalysis> SoaapAnalyses;
      static double PrivAccessProportion;
      static list<string> NoWarnLibs;
      static list<string> WarnLibs;
  
      template<typename T>
      static bool isSelected(T opt, list<T> optsList) {
        return find(optsList.begin(), optsList.end(), opt) != optsList.end();
      }
  };

}

#endif
