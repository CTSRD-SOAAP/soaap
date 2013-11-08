#include "llvm/Pass.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"

#include "soaap.h"

#include "SoaapPass.h"
#include "Common/CmdLineOpts.h"
#include "Common/Typedefs.h"
#include "Common/Sandbox.h"
#include "Analysis/GlobalVariableAnalysis.h"
#include "Analysis/VulnerabilityAnalysis.h"
#include "Analysis/PrivilegedCallAnalysis.h"
#include "Analysis/InfoFlow/AccessOriginAnalysis.h"
#include "Analysis/InfoFlow/SandboxPrivateAnalysis.h"
#include "Analysis/InfoFlow/ClassifiedAnalysis.h"
#include "Analysis/InfoFlow/CapabilityAnalysis.h"
#include "Instrument/PerformanceEmulationInstrumenter.h"
#include "Util/CallGraphUtils.h"
#include "Util/ClassHierarchyUtils.h"
#include "Util/ContextUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/SandboxUtils.h"

using namespace soaap;
using namespace llvm;
using namespace std;

void SoaapPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  AU.addRequired<CallGraph>();
  AU.addRequired<ProfileInfo>();
}

bool SoaapPass::runOnModule(Module& M) {
  outs() << "* Running " << getPassName();
  if (CmdLineOpts::ContextInsens) {
    outs() << " in context-insensitive mode";
    ContextUtils::setIsContextInsensitiveAnalysis(true);
  }
  outs() << "\n";

  CallGraph& CG = getAnalysis<CallGraph>();
  ProfileInfo& PI = getAnalysis<ProfileInfo>();
  LLVMAnalyses::setCallGraphAnalysis(&CG);
  LLVMAnalyses::setProfileInfoAnalysis(&PI);

  if (CmdLineOpts::ListFPCalls) {
    outs() << "* Listing function-pointer calls\n";
    CallGraphUtils::listFPCalls(M);
  }
  if (CmdLineOpts::ListAllFuncs) {
    CallGraphUtils::listAllFuncs(M);
    return true;
  }

  outs() << "* Finding class hierarchy (if there is one)\n";
  ClassHierarchyUtils::findClassHierarchy(M);

  outs() << "* Adding dynamic/annotated/inferred call edges to callgraph (if available)\n";
  CallGraphUtils::loadDynamicCallGraphEdges(M);
  CallGraphUtils::loadAnnotatedCallGraphEdges(M);

  if (CmdLineOpts::ListFPTargets) {
    CallGraphUtils::listFPTargets(M);
  }

  outs() << "* Processing command-line options\n"; 
  processCmdLineArgs(M);

  outs() << "* Finding sandboxes\n";
  findSandboxes(M);

  if (CmdLineOpts::ListSandboxedFuncs) {
    outs() << "* Listing sandboxed functions\n";
    SandboxUtils::outputSandboxedFunctions(sandboxes);
  }

  if (CmdLineOpts::EmPerf) {
    instrumentPerfEmul(M);
  }
  else {
    // do the checks statically
    outs() << "* Calculating privileged methods\n";
    calculatePrivilegedMethods(M);
    
    outs() << "* Checking global variable accesses\n";
    checkGlobalVariables(M);

    outs() << "* Checking file descriptor accesses\n";
    checkFileDescriptors(M);

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
  }

  return false;
}

void SoaapPass::processCmdLineArgs(Module& M) {
  // process ClVulnerableVendors
  for (StringRef vendor : CmdLineOpts::VulnerableVendors) {
    DEBUG(dbgs() << "Vulnerable vendor: " << vendor << "\n");
    vulnerableVendors.push_back(vendor);
  }
}

void SoaapPass::checkPrivilegedCalls(Module& M) {
  PrivilegedCallAnalysis analysis;
  analysis.doAnalysis(M, sandboxes);
}

void SoaapPass::checkLeakedRights(Module& M) {
  VulnerabilityAnalysis analysis(privilegedMethods, vulnerableVendors);
  analysis.doAnalysis(M, sandboxes);
}

void SoaapPass::checkOriginOfAccesses(Module& M) {
  AccessOriginAnalysis analysis(privilegedMethods);
  analysis.doAnalysis(M, sandboxes);
}

void SoaapPass::findSandboxes(Module& M) {
  sandboxes = SandboxUtils::findSandboxes(M);
}

void SoaapPass::checkPropagationOfSandboxPrivateData(Module& M) {
  SandboxPrivateAnalysis analysis(privilegedMethods);
  analysis.doAnalysis(M, sandboxes);
}

void SoaapPass::checkPropagationOfClassifiedData(Module& M) {
  ClassifiedAnalysis analysis;
  analysis.doAnalysis(M, sandboxes);
}

void SoaapPass::checkFileDescriptors(Module& M) {
  CapabilityAnalysis analysis;
  analysis.doAnalysis(M, sandboxes);
}

void SoaapPass::calculatePrivilegedMethods(Module& M) {
  privilegedMethods = SandboxUtils::getPrivilegedMethods(M);
}

void SoaapPass::checkGlobalVariables(Module& M) {
  GlobalVariableAnalysis analysis(privilegedMethods);
  analysis.doAnalysis(M, sandboxes);
}

void SoaapPass::instrumentPerfEmul(Module& M) {
  PerformanceEmulationInstrumenter instrumenter;
  instrumenter.instrument(M, sandboxes);
}

char SoaapPass::ID = 0;
static RegisterPass<SoaapPass> X("soaap", "Soaap Pass", false, false);

static void addPasses(const PassManagerBuilder &Builder, PassManagerBase &PM) {
  PM.add(new SoaapPass);
}

RegisterStandardPasses S(PassManagerBuilder::EP_OptimizerLast, addPasses);
