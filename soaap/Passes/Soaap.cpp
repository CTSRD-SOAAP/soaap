#include "llvm/Analysis/CallGraph.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"

#include "soaap.h"

#include "Soaap.h"
#include "Common/CmdLineOpts.h"
#include "Common/Typedefs.h"
#include "Common/Sandbox.h"
#include "Common/XO.h"
#include "Analysis/VulnerabilityAnalysis.h"
#include "Analysis/PrivilegedCallAnalysis.h"
#include "Analysis/CFGFlow/GlobalVariableAnalysis.h"
#include "Analysis/CFGFlow/SysCallsAnalysis.h"
#include "Analysis/InfoFlow/AccessOriginAnalysis.h"
#include "Analysis/InfoFlow/CapabilityAnalysis.h"
#include "Analysis/InfoFlow/CapabilitySysCallsAnalysis.h"
#include "Analysis/InfoFlow/ClassifiedAnalysis.h"
#include "Analysis/InfoFlow/SandboxPrivateAnalysis.h"
#include "Analysis/InfoFlow/RPC/RPCGraph.h"
#include "Instrument/PerformanceEmulationInstrumenter.h"
#include "OS/Sandbox/NoSandboxPlatform.h"
#include "OS/Sandbox/Capsicum.h"
#include "OS/Sandbox/SandboxPlatform.h"
#include "OS/Sandbox/Seccomp.h"
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

  XO::open_container("soaap");

  llvm::CallGraph& CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();
  LLVMAnalyses::setCallGraphAnalysis(&CG);

  outs() << "* Finding class hierarchy (if there is one)\n";
  ClassHierarchyUtils::findClassHierarchy(M);

  outs() << "* Adding dynamic/annotated/inferred call edges to callgraph (if available)\n";
  CallGraphUtils::loadDynamicCallGraphEdges(M);
  CallGraphUtils::loadAnnotatedCallGraphEdges(M);

  if (CmdLineOpts::ListAllFuncs) {
    CallGraphUtils::listAllFuncs(M);
    return true;
  }

  if (CmdLineOpts::DumpDOTCallGraph) {
    CallGraphUtils::dumpDOTGraph();
  }

  if (CmdLineOpts::ListFPTargets) {
    CallGraphUtils::listFPTargets(M);
  }

  outs() << "* Finding sandboxes\n";
  findSandboxes(M);

  if (CmdLineOpts::ListFPCalls) {
    outs() << "* Listing function-pointer calls\n";
    CallGraphUtils::listFPCalls(M, sandboxes);
  }
  if (CmdLineOpts::ListSandboxedFuncs) {
    outs() << "* Listing sandboxed functions\n";
    SandboxUtils::outputSandboxedFunctions(sandboxes);
  }

  if (CmdLineOpts::EmPerf) {
    outs() << "* Instrumenting sandbox emulation calls\n";
    instrumentPerfEmul(M);
  }
  else {
    // do the checks statically
    outs() << "* Calculating privileged methods\n";
    calculatePrivilegedMethods(M);
    if (!CmdLineOpts::SkipGlobalVariableAnalysis) {
      outs() << "* Checking global variable accesses\n";
      checkGlobalVariables(M);
    }

    //outs() << "* Checking file descriptor accesses\n";
    //checkFileDescriptors(M);

    outs() << "* Checking system calls\n";
    checkSysCalls(M);

    outs() << "* Checking propagation of data from sandboxes to privileged components\n";
    checkOriginOfAccesses(M);

    outs() << "* Checking propagation of classified data\n";
    checkPropagationOfClassifiedData(M);

    outs() << "* Checking propagation of sandbox-private data\n";
    checkPropagationOfSandboxPrivateData(M);

    outs() << "* Checking rights leaked by past vulnerable code\n";
    checkLeakedRights(M);

    outs() << "* Checking for calls to privileged functions from sandboxes\n";
    checkPrivilegedCalls(M);

    outs() << "* Building RPC graph\n";
    buildRPCGraph(M);
  }

  XO::close_container("soaap");
  XO::finish();

  return false;
}

void Soaap::processCmdLineArgs(Module& M) {
  // process ClVulnerableVendors
  for (StringRef vendor : CmdLineOpts::VulnerableVendors) {
    SDEBUG("soaap", 3, dbgs() << "Vulnerable vendor: " << vendor << "\n");
    vulnerableVendors.push_back(vendor);
  }

  // process ClSandboxPlatform
  SDEBUG("soaap", 3, dbgs() << "Sandboxing model: " << CmdLineOpts::SandboxPlatform << "\n");
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
      sandboxPlatform.reset(new class Capsicum);
      break;
    }
    case SandboxPlatformName::Seccomp: {
      sandboxPlatform.reset(new class Seccomp);
      break;
    }
    default: {
      errs() << "Unrecognised Sandbox Platform " << CmdLineOpts::SandboxPlatform << "\n";
    }
  }

  // process ClReportOutputFormats
  // default value is text
  // TODO: not sure how to specify this in the option itself
  if (CmdLineOpts::ReportOutputFormats.empty()) { 
    CmdLineOpts::ReportOutputFormats.push_back(Text);
  }
  for (ReportOutputFormat r : CmdLineOpts::ReportOutputFormats) {
    switch (r) {
      case Text: {
        SDEBUG("soaap", 3, dbgs() << "Text selected\n");
        XO::create(XO_STYLE_TEXT, XOF_FLUSH);
        break;
      }
      case HTML: {
        SDEBUG("soaap", 3, dbgs() << "HTML selected\n");
        string filename = CmdLineOpts::ReportFilePrefix + ".html";
        if (FILE* fp = fopen(filename.c_str(), "w")) {
          XO::create_to_file(fp, XO_STYLE_HTML, XOF_PRETTY | XOF_FLUSH);
        }
        else {
          errs() << "Error creating JSON report file: " << strerror(errno) << "\n";
        }
        break;
      }
      case JSON: {
        SDEBUG("soaap", 3, dbgs() << "JSON selected\n");
        string filename = CmdLineOpts::ReportFilePrefix + ".json";
        SDEBUG("soaap", 3, dbgs() << "Opening file \"" << filename << "\"\n");
        if (FILE* fp = fopen(filename.c_str(), "w")) {
          XO::create_to_file(fp, XO_STYLE_JSON, XOF_PRETTY | XOF_FLUSH);
        }
        else {
          errs() << "Error creating JSON report file: " << strerror(errno) << "\n";
        }
        break;
      }
      case XML: {
        SDEBUG("soaap", 3, dbgs() << "XML selected\n");
        string filename = CmdLineOpts::ReportFilePrefix + ".xml";
        SDEBUG("soaap", 3, dbgs() << "Opening file \"" << filename << "\"\n");
        if (FILE* fp = fopen(filename.c_str(), "w")) {
          XO::create_to_file(fp, XO_STYLE_XML, XOF_PRETTY | XOF_FLUSH);
        }
        else {
          errs() << "Error creating XML report file: " << strerror(errno) << "\n";
        }
        break;
      }
      default: { }
    }
  }
}

void Soaap::checkPrivilegedCalls(Module& M) {
  PrivilegedCallAnalysis analysis;
  analysis.doAnalysis(M, sandboxes);
}

void Soaap::checkLeakedRights(Module& M) {
  VulnerabilityAnalysis analysis(privilegedMethods, vulnerableVendors, sandboxPlatform);
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
  SandboxPrivateAnalysis analysis(CmdLineOpts::ContextInsens, privilegedMethods);
  analysis.doAnalysis(M, sandboxes);
}

void Soaap::checkPropagationOfClassifiedData(Module& M) {
  ClassifiedAnalysis analysis(CmdLineOpts::ContextInsens);
  analysis.doAnalysis(M, sandboxes);
}

void Soaap::checkFileDescriptors(Module& M) {
  CapabilityAnalysis analysis(CmdLineOpts::ContextInsens);
  analysis.doAnalysis(M, sandboxes);
}

void Soaap::checkSysCalls(Module& M) {
  SysCallsAnalysis sysCallsAnalysis(sandboxPlatform);
  sysCallsAnalysis.doAnalysis(M, sandboxes);
  CapabilitySysCallsAnalysis capsAnalysis(CmdLineOpts::ContextInsens, sandboxPlatform, sysCallsAnalysis);
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

void Soaap::buildRPCGraph(Module& M) {
  RPCGraph G;
  G.build(sandboxes, privilegedMethods, M);
  if (CmdLineOpts::DumpRPCGraph) {
    G.dump(M);
  }
}

char Soaap::ID = 0;
INITIALIZE_PASS(Soaap, "soaap", "Soaap Pass", false, false);
