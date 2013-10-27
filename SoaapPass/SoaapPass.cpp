#include "llvm/Pass.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"
#include "llvm/Support/CommandLine.h"

#include "soaap.h"

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

using namespace llvm;
using namespace std;

static cl::list<string> ClVulnerableVendors("soaap-vulnerable-vendors",
       cl::desc("Comma-separated list of vendors whose code should "
                "be treated as vulnerable"),
       cl::value_desc("list of vendors"), cl::CommaSeparated);

static cl::opt<bool> ClEmPerf("soaap-emulate-performance",
       cl::desc("Emulate sandboxing performance"));

static cl::opt<bool> ClContextInsens("soaap-context-insens",
       cl::desc("Don't use context-sensitive analysis"));

static cl::opt<bool> ClListSandboxedFuncs("soaap-list-sandboxed-funcs",
       cl::desc("List sandboxed functions"));

static cl::opt<bool> ClListFPCalls("soaap-list-fp-calls",
       cl::desc("List function-pointer calls"));

static cl::opt<bool> ClListAllFuncs("soaap-list-all-funcs",
       cl::desc("List all functions"));

static cl::opt<string> ClDumpVirtualCallees("soaap-dump-virtual-callees",
       cl::desc("Dump C++ virtual callees (derived from debugging information) to file"), cl::value_desc("filename"));

static cl::opt<string> ClReadVirtualCallees("soaap-read-virtual-callees",
       cl::desc("Read C++ virtual callees from file"), cl::value_desc("filename"));

namespace soaap {

  struct SoaapPass : public ModulePass {

    static char ID;

    SandboxVector sandboxes;
    FunctionVector privilegedMethods;
    StringVector vulnerableVendors;

    SoaapPass() : ModulePass(ID) { }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
      AU.addRequired<CallGraph>();
      AU.addRequired<ProfileInfo>();
    }

    virtual bool runOnModule(Module& M) {
      outs() << "* Running " << getPassName();
      if (ClContextInsens) {
        outs() << " in context-insensitive mode";
        ContextUtils::setIsContextInsensitiveAnalysis(true);
      }
      outs() << "\n";
 
      CallGraph& CG = getAnalysis<CallGraph>();
      ProfileInfo& PI = getAnalysis<ProfileInfo>();
      LLVMAnalyses::setCallGraphAnalysis(&CG);
      LLVMAnalyses::setProfileInfoAnalysis(&PI);

      if (ClListFPCalls) {
        outs() << "* Listing function-pointer calls\n";
        CallGraphUtils::listFPCalls(M);
      }
      if (ClListAllFuncs) {
        CallGraphUtils::listAllFuncs(M);
        return true;
      }

      outs() << "* Finding class hierarchy (if there is one)\n";
      ClassHierarchyUtils::findClassHierarchy(M);

      if (ClDumpVirtualCallees != "") {
        outs() << "* Dumping virtual callee information to file\n";
        ClassHierarchyUtils::cacheAllCalleesForVirtualCalls(M);
        ClassHierarchyUtils::dumpVirtualCalleeInformation(M, ClDumpVirtualCallees);
        return true;
      }
      else if (ClReadVirtualCallees != "") {
        outs() << "* Reading virtual callee information from file\n";
        ClassHierarchyUtils::readVirtualCalleeInformation(M, ClReadVirtualCallees);
        return true;
      }

      outs() << "* Adding dynamic/annotated call edges to callgraph (if available)\n";
      CallGraphUtils::loadDynamicCallGraphEdges(M);
      CallGraphUtils::loadAnnotatedCallGraphEdges(M);

      outs() << "* Processing command-line options\n"; 
      processCmdLineArgs(M);

      outs() << "* Finding sandboxes\n";
      findSandboxes(M);

      if (ClListSandboxedFuncs) {
        outs() << "* Listing sandboxed functions\n";
        SandboxUtils::outputSandboxedFunctions(sandboxes);
      }

      if (ClEmPerf) {
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

    void processCmdLineArgs(Module& M) {
      // process ClVulnerableVendors
      for (StringRef vendor : ClVulnerableVendors) {
        DEBUG(dbgs() << "Vulnerable vendor: " << vendor << "\n");
        vulnerableVendors.push_back(vendor);
      }
    }

    void checkPrivilegedCalls(Module& M) {
      PrivilegedCallAnalysis analysis;
      analysis.doAnalysis(M, sandboxes);
    }

    void checkLeakedRights(Module& M) {
      VulnerabilityAnalysis analysis(privilegedMethods, vulnerableVendors);
      analysis.doAnalysis(M, sandboxes);
    }

    void checkOriginOfAccesses(Module& M) {
      AccessOriginAnalysis analysis(privilegedMethods);
      analysis.doAnalysis(M, sandboxes);
    }

    void findSandboxes(Module& M) {
      sandboxes = SandboxUtils::findSandboxes(M);
    }

    void checkPropagationOfSandboxPrivateData(Module& M) {
      SandboxPrivateAnalysis analysis(privilegedMethods);
      analysis.doAnalysis(M, sandboxes);
    }

    void checkPropagationOfClassifiedData(Module& M) {
      ClassifiedAnalysis analysis;
      analysis.doAnalysis(M, sandboxes);
    }

    void checkFileDescriptors(Module& M) {
      CapabilityAnalysis analysis;
      analysis.doAnalysis(M, sandboxes);
    }

    void calculatePrivilegedMethods(Module& M) {
      privilegedMethods = SandboxUtils::getPrivilegedMethods(M);
    }

    void checkGlobalVariables(Module& M) {
      GlobalVariableAnalysis analysis(privilegedMethods);
      analysis.doAnalysis(M, sandboxes);
    }

    void instrumentPerfEmul(Module& M) {
      PerformanceEmulationInstrumenter instrumenter;
      instrumenter.instrument(M, sandboxes);
    }
  };

  char SoaapPass::ID = 0;
  static RegisterPass<SoaapPass> X("soaap", "Soaap Pass", false, false);

  void addPasses(const PassManagerBuilder &Builder, PassManagerBase &PM) {
    PM.add(new SoaapPass);
  }

  RegisterStandardPasses S(PassManagerBuilder::EP_OptimizerLast, addPasses);
}
