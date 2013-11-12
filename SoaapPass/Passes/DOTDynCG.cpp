#include "DOTDynCG.h"

#include <iostream>
#include <vector>

using namespace llvm;
using namespace std;

namespace soaap {

  void DOTDynCG::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<CallGraph>();
    AU.addRequired<ProfileInfo>();
  }

  bool DOTDynCG::runOnModule(Module& M) {
    outs() << "Running output dynamic callgraph pass!\n";

    ProfileInfo &PI = getAnalysis<ProfileInfo>();

    DEBUG(dbgs() << "Static callgraph edges:\n");
    CallGraph& CG = getAnalysis<CallGraph>();
    for (CallGraph::iterator CI = CG.begin(), CE = CG.end(); CI != CE; CI++) {
      CallGraphNode* node = CI->second;
      if (Function* callerFunc = node->getFunction()) {
        for (CallGraphNode::iterator NI=node->begin(), NE=node->end(); NI != NE; NI++) {
          if (CallGraphNode* calleeNode = NI->second) {
            if (Function* calleeFunc = calleeNode->getFunction()) {
              DEBUG(dbgs() << "\t" << callerFunc->getName().str() << " -> " << calleeFunc->getName().str() << "\n");
            }
          }  
        }
      }
    }
    DEBUG(dbgs() << "\n\n");

    // addCalledFunction requires the call-site. we don't have
    // this information so we just construct a fake one
    DEBUG(dbgs() << "Initialising DynamicInstruction\n");
    DynamicInstruction = constructDummyCallInst(M);

    // dynamic call graph edges
    DEBUG(dbgs() << "Dynamic callgraph edges:\n");
    for (Module::iterator F1 = M.begin(), E1 = M.end(); F1 != E1; ++F1) {
        if (F1->isDeclaration()) continue;
        //outs() << "F1: " << F1->getName() << "\n";
        for (inst_iterator I = inst_begin(F1), E = inst_end(F1); I != E; ++I) {
          if (CallInst* C = dyn_cast<CallInst>(&*I)) {
            for (const Function* F2 : PI.getDynamicCallees(C)) {
              //outs() << "    F2: " << F2->getName() << "\n";
              if (F2->isDeclaration()) continue;
              if (PI.isDynamicCallEdge(C, F2)) {
                // add dynamic edge to CallGraph
                DEBUG(dbgs() << "Inserting dynamic edge for " << F1->getName() << " -> " << F2->getName() << "\n");
                CallGraphNode* F1Node = CG.getOrInsertFunction(F1);
                CallGraphNode* F2Node = CG.getOrInsertFunction(F2);
                F1Node->addCalledFunction(CallSite(DynamicInstruction), F2Node);
              }
            }
          }
        }
      }

      DEBUG(CG.print(dbgs(), &M));
      DOTGraphTraits<CallGraph*> DT;
      outs() << DT.getGraphName(NULL) << "\n";
      WriteGraph(&getAnalysis<CallGraph>(), "callgraph");
      return true;
  }

  /* 
   * Create a dummy call instruction for using to identify a dynamic call edge 
   *
   * void DummyFunc() {
   *   call @DummyFunc();
   *   return;
   * }
   * 
   * We have to create a new function to contain this dummy call because otherwise
   * the function's destructor will fail as a use of the function will still exist,
   * namely this dummy call we are creating. Function destructors are called by the
   * Module destructor. Before the Module does this though, it ensures that Users 
   * drop their references thus allowing Functions to be deleted. Otherwise, LLVM
   * will spit out the following error:
   *
   *   While deleting: void ()* %DummyFunc
   *    Use still stuck around after Def is destroyed:  call void @DummyFunc()
   *    Assertion failed: (use_empty() && "Uses remain when a value is destroyed!"), 
   *    function ~Value, file lib/VMCore/Value.cpp, line 75.
   */

  CallInst* DOTDynCG::constructDummyCallInst(Module& M) {
    std::vector<Type*> FuncParamTypes;
    FunctionType* FuncType = FunctionType::get(Type::getVoidTy(M.getContext()), FuncParamTypes, false);
    Function* DummyFunc = Function::Create(FuncType, GlobalValue::ExternalLinkage, "DummyFunc", &M);
    BasicBlock* BB = BasicBlock::Create(M.getContext(), "", DummyFunc, 0);
    CallInst* CI = CallInst::Create(DummyFunc, "", BB);
    ReturnInst::Create(M.getContext(), BB);
    return CI;
  }

char DOTDynCG::ID = 0;
Instruction* DOTDynCG::DynamicInstruction = 0;
static RegisterPass<DOTDynCG> X("dot-dynamic-callgraph", "Output dynamic callgraph pass", false, false);

void addPasses(const PassManagerBuilder &Builder, PassManagerBase &PM) {
  PM.add(new DOTDynCG);
}

RegisterStandardPasses S(PassManagerBuilder::EP_OptimizerLast, addPasses);
}


