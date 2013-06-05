#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Regex.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"
#include "llvm/ADT/ilist.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/DebugInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Analysis/ProfileInfo.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CFG.h"

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
#include "Utils/CallGraphUtils.h"
#include "Utils/LLVMAnalyses.h"
#include "Utils/SandboxUtils.h"
#include "Utils/ClassifiedUtils.h"
#include "Utils/PrettyPrinters.h"

#include <iostream>
#include <vector>
#include <climits>
#include <functional>

using namespace llvm;
using namespace std;

static cl::list<std::string> ClVulnerableVendors("soaap-vulnerable-vendors",
       cl::desc("Comma-separated list of vendors whose code should "
                "be treated as vulnerable"),
       cl::value_desc("list of vendors"), cl::CommaSeparated);

static cl::opt<bool> ClEmPerf("soaap-emulate-performance",
       cl::desc("Emulate sandboxing performance"));

namespace soaap {

  struct SoaapPass : public ModulePass {

    static char ID;

    SandboxVector sandboxes;
    FunctionVector privilegedMethods;
    StringVector vulnerableVendors;

    SoaapPass() : ModulePass(ID) { }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      if (!ClEmPerf) { 
        AU.setPreservesCFG();
        AU.addRequired<CallGraph>();
        AU.addRequired<ProfileInfo>();
      }
    }

    virtual bool runOnModule(Module& M) {

      outs() << "* Running " << getPassName() << "\n";
    
      CallGraph& CG = getAnalysis<CallGraph>();
      ProfileInfo& PI = getAnalysis<ProfileInfo>();
      LLVMAnalyses::setCallGraphAnalysis(&CG);
      LLVMAnalyses::setProfileInfoAnalysis(&PI);
      
      outs() << "* Adding dynamic call edges to callgraph (if available)\n";
      CallGraphUtils::loadDynamicCallGraphEdges(M);

      outs() << "* Processing command-line options\n"; 
      processCmdLineArgs(M);

      outs() << "* Finding sandboxes\n";
      findSandboxes(M);

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

      //WORKAROUND: remove calls to llvm.ptr.annotate.p0i8, otherwise LLVM will
      //            crash when generating object code.
      if (Function* F = M.getFunction("llvm.ptr.annotation.p0i8")) {
        outs() << "BUG WORKAROUND: Removing calls to intrinisc @llvm.ptr.annotation.p0i8\n";
        for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
          IntrinsicInst* intrinsicCall = dyn_cast<IntrinsicInst>(u.getUse().getUser());
          BasicBlock::iterator ii(intrinsicCall);
          ReplaceInstWithValue(intrinsicCall->getParent()->getInstList(), ii, intrinsicCall->getOperand(0));
        }   
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
      privilegedMethods = SandboxUtils::calculatePrivilegedMethods(M);
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
