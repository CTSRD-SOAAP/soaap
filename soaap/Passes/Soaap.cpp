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

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "soaap.h"

#include "Soaap.h"
#include "Common/CmdLineOpts.h"
#include "Common/Typedefs.h"
#include "Common/Sandbox.h"
#include "Common/XO.h"
#include "Analysis/VulnerabilityAnalysis.h"
#include "Analysis/PrivilegedCallAnalysis.h"
#include "Analysis/SandboxedFuncAnalysis.h"
#include "Analysis/CFGFlow/GlobalVariableAnalysis.h"
#include "Analysis/CFGFlow/SysCallsAnalysis.h"
#include "Analysis/InfoFlow/AccessOriginAnalysis.h"
#include "Analysis/InfoFlow/CapabilityAnalysis.h"
#include "Analysis/InfoFlow/CapabilitySysCallsAnalysis.h"
#include "Analysis/InfoFlow/ClassifiedAnalysis.h"
#include "Analysis/InfoFlow/SandboxPrivateAnalysis.h"
#include "Analysis/InfoFlow/RPC/RPCGraph.h"
#include "Generate/SandboxGenerator.h"
#include "Instrument/PerformanceEmulationInstrumenter.h"
#include "OS/FreeBSDSysCallProvider.h"
#include "OS/LinuxSysCallProvider.h"
#include "OS/Sandbox/NoSandboxPlatform.h"
#include "OS/Sandbox/Capsicum.h"
#include "OS/Sandbox/SandboxPlatform.h"
#include "OS/Sandbox/Seccomp.h"
#include "OS/Sandbox/SeccompBPF.h"
#include "Util/CallGraphUtils.h"
#include "Util/ClassHierarchyUtils.h"
#include "Util/ContextUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/SandboxUtils.h"

#include <cstdio>

using namespace soaap;
using namespace llvm;
using namespace std;

void Soaap::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  AU.addRequired<CallGraphWrapperPass>();
}

bool Soaap::runOnModule(Module& M) {
  outs() << "* Running " << getPassName();
  if (CmdLineOpts::ContextInsens) {
    outs() << " in context-insensitive mode";
  }
  outs() << "\n";
  
  outs() << "* Processing command-line options\n"; 
  processCmdLineArgs(M);

  XO::Container soaapContainer("soaap");

  llvm::CallGraph& CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();
  LLVMAnalyses::setCallGraphAnalysis(&CG);
  
  outs() << "* Finding class hierarchy (if there is one)\n";
  ClassHierarchyUtils::findClassHierarchy(M);

  outs() << "* Finding sandboxes\n";
  findSandboxes(M);

  outs() << "* Building basic callgraph\n";
  CallGraphUtils::buildBasicCallGraph(M, sandboxes);
  
  CallGraphUtils::warnUnresolvedFuncs(M);
  
  outs() << "* Calculating privileged methods\n";
  calculatePrivilegedMethods(M);

  outs() << "* Reinitialising sandboxes\n";
  SandboxUtils::reinitSandboxes(sandboxes);

  outs() << "* Adding annotated/inferred call edges to callgraph (if available)\n";
  CallGraphUtils::loadAnnotatedInferredCallGraphEdges(M, sandboxes);
 
  // reobtain privileged methods
  privilegedMethods = SandboxUtils::getPrivilegedMethods(M);

  outs() << "* Validating sandbox creation points\n";
  SandboxUtils::validateSandboxCreations(sandboxes);
  
  if (CmdLineOpts::ListAllFuncs) {
    CallGraphUtils::listAllFuncs(M);
    return true;
  }

  if (CmdLineOpts::DumpDOTCallGraph) {
    CallGraphUtils::dumpDOTGraph();
  }

  if (CmdLineOpts::ListFPTargets) {
    CallGraphUtils::listFPTargets(M, sandboxes);
  }

  if (CmdLineOpts::ListFPCalls) {
    outs() << "* Listing function-pointer calls\n";
    CallGraphUtils::listFPCalls(M, sandboxes);
  }
  if (CmdLineOpts::ListSandboxedFuncs) {
    outs() << "* Listing sandboxed functions\n";
    SandboxUtils::outputSandboxedFunctions(sandboxes);
  }
  
  if (CmdLineOpts::ListPrivilegedFuncs) {
    outs() << "\n* Listing privileged functions\n";
    SandboxUtils::outputPrivilegedFunctions();
  }

  if (CmdLineOpts::EmPerf) {
    outs() << "* Instrumenting sandbox emulation calls\n";
    instrumentPerfEmul(M);
  } 
  else if (CmdLineOpts::GenSandboxes) {

    outs() << "* Building RPC graph\n";
    buildRPCGraph(M);
    
    if (CmdLineOpts::isSelected(SoaapAnalysis::Vuln, CmdLineOpts::SoaapAnalyses)) {
      outs() << "* Checking rights leaked by past vulnerable code\n";
      checkLeakedRights(M);
    }
    
    if (CmdLineOpts::isSelected(SoaapAnalysis::Globals, CmdLineOpts::SoaapAnalyses)) {
      outs() << "* Checking global variable accesses\n";
      checkGlobalVariables(M);
    }

    //outs() << "* Checking file descriptor accesses\n";
    //checkFileDescriptors(M);

    if (CmdLineOpts::isSelected(SoaapAnalysis::SysCalls, CmdLineOpts::SoaapAnalyses)) {
      outs() << "* Checking system calls\n";
      checkSysCalls(M);
    }
  
    if (CmdLineOpts::isSelected(SoaapAnalysis::PrivCalls, CmdLineOpts::SoaapAnalyses)) {
      outs() << "* Checking for calls to privileged functions from sandboxes\n";
      checkPrivilegedCalls(M);
    }

    if (CmdLineOpts::isSelected(SoaapAnalysis::SandboxedFuncs, CmdLineOpts::SoaapAnalyses)) {
      outs() << "* Checking sandbox-only functions\n";
      checkSandboxedFuncs(M);
    }

    if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::SoaapAnalyses)) {
      outs() << "* Checking propagation of data from sandboxes to privileged components\n";
      checkOriginOfAccesses(M);

      outs() << "* Checking propagation of classified data\n";
      checkPropagationOfClassifiedData(M);

      outs() << "* Checking propagation of sandbox-private data\n";
      checkPropagationOfSandboxPrivateData(M);
    }
    outs() << "* Generating sandboxes\n";
    generateSandboxes(M);
  } 
  else {
    outs() << "* Building RPC graph\n";
    buildRPCGraph(M);
    
    if (CmdLineOpts::isSelected(SoaapAnalysis::Vuln, CmdLineOpts::SoaapAnalyses)) {
      outs() << "* Checking rights leaked by past vulnerable code\n";
      checkLeakedRights(M);
    }
    
    if (CmdLineOpts::isSelected(SoaapAnalysis::Globals, CmdLineOpts::SoaapAnalyses)) {
      outs() << "* Checking global variable accesses\n";
      checkGlobalVariables(M);
    }

    //outs() << "* Checking file descriptor accesses\n";
    //checkFileDescriptors(M);

    if (CmdLineOpts::isSelected(SoaapAnalysis::SysCalls, CmdLineOpts::SoaapAnalyses)) {
      outs() << "* Checking system calls\n";
      checkSysCalls(M);
    }
  
    if (CmdLineOpts::isSelected(SoaapAnalysis::PrivCalls, CmdLineOpts::SoaapAnalyses)) {
      outs() << "* Checking for calls to privileged functions from sandboxes\n";
      checkPrivilegedCalls(M);
    }

    if (CmdLineOpts::isSelected(SoaapAnalysis::SandboxedFuncs, CmdLineOpts::SoaapAnalyses)) {
      outs() << "* Checking sandbox-only functions\n";
      checkSandboxedFuncs(M);
    }

    if (CmdLineOpts::isSelected(SoaapAnalysis::InfoFlow, CmdLineOpts::SoaapAnalyses)) {
      outs() << "* Checking propagation of data from sandboxes to privileged components\n";
      checkOriginOfAccesses(M);

      outs() << "* Checking propagation of classified data\n";
      checkPropagationOfClassifiedData(M);

      outs() << "* Checking propagation of sandbox-private data\n";
      checkPropagationOfSandboxPrivateData(M);
    }
    
  }
  
  CallGraphUtils::emitTraceReferences();

  soaapContainer.close();
  XO::finish();

  return false;
}

void Soaap::generateSandboxingModules(vector<Module*>* MV) {
  genModules = MV;
}

void Soaap::processCmdLineArgs(Module& M) {
  // process ClOperatingSystem
  switch (CmdLineOpts::OperatingSystem) {
    case OperatingSystemName::FreeBSD: {
      operatingSystem.reset(new class FreeBSDSysCallProvider);
      break;
    }
    case OperatingSystemName::Linux: {
      operatingSystem.reset(new class LinuxSysCallProvider);
      break;
    }
  }

  if (operatingSystem) {
    operatingSystem->initSysCalls();
  }

  // process ClSandboxPlatform
  switch (CmdLineOpts::SandboxPlatform) {
    case SandboxPlatformName::None: {
      // no sandboxing
      sandboxPlatform.reset(new class NoSandboxPlatform);
      break;
    }
    case SandboxPlatformName::Annotated: {
      // don't initialise sandboxPlatform
      break;
    }
    case SandboxPlatformName::Capsicum: {
      if (CmdLineOpts::OperatingSystem != OperatingSystemName::FreeBSD) {
        errs() << "ERROR: Capsicum is only currently being modelled for FreeBSD, please specify --soaap-os=freebsd";
        exit(-1);
      }
      sandboxPlatform.reset(new class Capsicum);
      break;
    }
    case SandboxPlatformName::Seccomp: {
      if (CmdLineOpts::OperatingSystem != OperatingSystemName::Linux) {
        errs() << "ERROR: Seccomp is only currently being modelled for Linux, please specify --soaap-os=linux";
        exit(-1);
      }
      sandboxPlatform.reset(new class Seccomp);
      break;
    }
    case SandboxPlatformName::SeccompBPF: {
      if (CmdLineOpts::OperatingSystem != OperatingSystemName::Linux) {
        errs() << "ERROR: Seccomp-BPF is only currently being modelled for Linux, please specify --soaap-os=linux";
        exit(-1);
      }
      if (CmdLineOpts::SandboxPolicy.empty()) {
        errs() << "WARNING: No seccomp sandbox policy file specified, will assume deny-all semantics\n";
      }
      sandboxPlatform.reset(new class SeccompBPF);
      break;
    }
    default: {
      errs() << "Unrecognised Sandbox Platform\n";
    }
  }

  // process ClReportOutputFormats
  // default value is text
  // TODO: not sure how to specify this in the option itself
  if (CmdLineOpts::ReportOutputFormats.empty()) { 
    CmdLineOpts::ReportOutputFormats.push_back(ReportOutputFormat::Text);
  }

  const int XoFlags = (CmdLineOpts::PrettyPrint ? XOF_PRETTY : 0);

  for (ReportOutputFormat r : CmdLineOpts::ReportOutputFormats) {
    switch (r) {
      case ReportOutputFormat::Text: {
        SDEBUG("soaap", 3, dbgs() << "Text selected\n");
        XO::create(XO_STYLE_TEXT, XoFlags | XOF_FLUSH);
        break;
      }
      case ReportOutputFormat::HTML: {
        SDEBUG("soaap", 3, dbgs() << "HTML selected\n");
        string filename = CmdLineOpts::ReportFilePrefix + ".html";
        if (FILE* fp = fopen(filename.c_str(), "w")) {
          XO::create_to_file(fp, XO_STYLE_HTML, XoFlags);
        }
        else {
          errs() << "Error creating JSON report file: " << strerror(errno) << "\n";
        }
        break;
      }
      case ReportOutputFormat::JSON: {
        SDEBUG("soaap", 3, dbgs() << "JSON selected\n");
        string filename = CmdLineOpts::ReportFilePrefix + ".json";
        SDEBUG("soaap", 3, dbgs() << "Opening file \"" << filename << "\"\n");
        if (FILE* fp = fopen(filename.c_str(), "w")) {
          XO::create_to_file(fp, XO_STYLE_JSON, XoFlags);
        }
        else {
          errs() << "Error creating JSON report file: " << strerror(errno) << "\n";
        }
        break;
      }
      case ReportOutputFormat::XML: {
        SDEBUG("soaap", 3, dbgs() << "XML selected\n");
        string filename = CmdLineOpts::ReportFilePrefix + ".xml";
        SDEBUG("soaap", 3, dbgs() << "Opening file \"" << filename << "\"\n");
        if (FILE* fp = fopen(filename.c_str(), "w")) {
          XO::create_to_file(fp, XO_STYLE_XML, XoFlags);
        }
        else {
          errs() << "Error creating XML report file: " << strerror(errno) << "\n";
        }
        break;
      }
      default: { }
    }
  }

  if (CmdLineOpts::OutputTraces.empty()) {
    // "vulnerability" is the default
    CmdLineOpts::OutputTraces.push_back(SoaapAnalysis::Vuln);
  }
  else if (CmdLineOpts::isSelected(SoaapAnalysis::All, CmdLineOpts::OutputTraces)) {
    // expand "all" to all analyses
    CmdLineOpts::OutputTraces.clear();
    CmdLineOpts::OutputTraces.push_back(SoaapAnalysis::Vuln);
    CmdLineOpts::OutputTraces.push_back(SoaapAnalysis::Globals);
    CmdLineOpts::OutputTraces.push_back(SoaapAnalysis::SysCalls);
    CmdLineOpts::OutputTraces.push_back(SoaapAnalysis::PrivCalls);
    CmdLineOpts::OutputTraces.push_back(SoaapAnalysis::SandboxedFuncs);
    CmdLineOpts::OutputTraces.push_back(SoaapAnalysis::InfoFlow);
  }

  if (CmdLineOpts::SoaapAnalyses.empty()
     || CmdLineOpts::isSelected(SoaapAnalysis::All, CmdLineOpts::SoaapAnalyses)) {
    // "all" is the default, also expand "all" to all analyses
    CmdLineOpts::SoaapAnalyses.clear();
    CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::Vuln);
    CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::Globals);
    CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::SysCalls);
    CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::PrivCalls);
    CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::SandboxedFuncs);
    CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::InfoFlow);
  }

  if (CmdLineOpts::Mode != SoaapMode::Custom) {
    CmdLineOpts::SoaapAnalyses.clear();
    switch (CmdLineOpts::Mode) {
      case SoaapMode::All: {
        CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::Vuln);
        CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::Globals);
        CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::SysCalls);
        CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::PrivCalls);
        CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::SandboxedFuncs);
        CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::InfoFlow);
        break;
      }
      case SoaapMode::Vuln: {
        CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::Vuln);
        break;
      }
      case SoaapMode::Correct: {
        CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::Globals);
        CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::SysCalls);
        CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::PrivCalls);
        CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::SandboxedFuncs);
        break;
      }
      case SoaapMode::InfoFlow: {
        CmdLineOpts::SoaapAnalyses.push_back(SoaapAnalysis::InfoFlow);
        break;
      }
      default: { }
    }
  }

  if (!CmdLineOpts::WarnLibs.empty() && !CmdLineOpts::NoWarnLibs.empty()) {
    errs() << "ERROR: can only specify one of --soaap-warn-modules and --soaap-nowarn-modules\n";
  }
}

void Soaap::checkPrivilegedCalls(Module& M) {
  PrivilegedCallAnalysis analysis;
  analysis.doAnalysis(M, sandboxes);
}

void Soaap::checkSandboxedFuncs(Module& M) {
  SandboxedFuncAnalysis analysis;
  analysis.doAnalysis(M, sandboxes);
}

void Soaap::checkLeakedRights(Module& M) {
  VulnerabilityAnalysis analysis(privilegedMethods, sandboxPlatform);
  analysis.doAnalysis(M, sandboxes);
}

void Soaap::checkOriginOfAccesses(Module& M) {
  AccessOriginAnalysis analysis(CmdLineOpts::ContextInsens, privilegedMethods);
  analysis.doAnalysis(M, sandboxes);
}

void Soaap::findSandboxes(Module& M) {
  sandboxes = SandboxUtils::findSandboxes(M);
}

void Soaap::checkPropagationOfSandboxPrivateData(Module& M) {
  SandboxPrivateAnalysis analysis(CmdLineOpts::ContextInsens, privilegedMethods, sandboxes);
  analysis.doAnalysis(M, sandboxes);
}

void Soaap::checkPropagationOfClassifiedData(Module& M) {
  ClassifiedAnalysis analysis(CmdLineOpts::ContextInsens);
  analysis.doAnalysis(M, sandboxes);
}

void Soaap::checkFileDescriptors(Module& M) {
  CapabilityAnalysis analysis(CmdLineOpts::ContextInsens, operatingSystem);
  analysis.doAnalysis(M, sandboxes);
}

void Soaap::checkSysCalls(Module& M) {
  SysCallsAnalysis sysCallsAnalysis(sandboxPlatform, operatingSystem);
  sysCallsAnalysis.doAnalysis(M, sandboxes);
  CapabilitySysCallsAnalysis capsAnalysis(CmdLineOpts::ContextInsens, sandboxPlatform, operatingSystem, sysCallsAnalysis);
  capsAnalysis.doAnalysis(M, sandboxes);
}

void Soaap::calculatePrivilegedMethods(Module& M) {
  privilegedMethods = SandboxUtils::getPrivilegedMethods(M);
}

void Soaap::checkGlobalVariables(Module& M) {
  GlobalVariableAnalysis analysis(privilegedMethods);
  analysis.doAnalysis(M, sandboxes);
}

void Soaap::instrumentPerfEmul(Module& M) {
  PerformanceEmulationInstrumenter instrumenter;
  instrumenter.instrument(M, sandboxes);
}

void Soaap::generateSandboxes(Module& M) {
  SandboxGenerator generator;
  generator.generate(M, sandboxes, *genModules);
}

void Soaap::buildRPCGraph(Module& M) {
  RPCGraph G;
  G.build(sandboxes, privilegedMethods, M);
  if (CmdLineOpts::DumpRPCGraph) {
    G.dump(M);
  }
}

char Soaap::ID = 0;
INITIALIZE_PASS(Soaap, "soaap", "Soaap Pass", false, false);
