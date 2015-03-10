#ifndef SOAAP_ANALYSIS_CFGFLOW_CFGFLOWANALYSIS_H
#define SOAAP_ANALYSIS_CFGFLOW_CFGFLOWANALYSIS_H

#include <map>
#include <list>

#include "llvm/ADT/SetVector.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Pass.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/Local.h"

#include "ADT/QueueSet.h"
#include "Analysis/Analysis.h"
#include "Common/CmdLineOpts.h"
#include "Common/Debug.h"
#include "Util/CallGraphUtils.h"
#include "Util/ContextUtils.h"
#include "Util/DebugUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/SandboxUtils.h"

using namespace std;
using namespace llvm;

namespace soaap {
  // Base class for CFG-oriented data flow analysis
  template<class FactType>
  class CFGFlowAnalysis : public Analysis {
    public:
      virtual void doAnalysis(Module& M, SandboxVector& sandboxes);

    protected:
      map<Instruction*,FactType> state;
      virtual void initialise(QueueSet<BasicBlock*>& worklist, Module& M, SandboxVector& sandboxes) = 0;
      virtual void performDataFlowAnalysis(QueueSet<BasicBlock*>&, SandboxVector& sandboxes, Module& M);
      virtual void updateStateAndPropagate(Instruction* I, FactType val, QueueSet<BasicBlock*>& worklist);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) = 0;
      virtual FactType bottomValue() = 0;
      virtual string stringifyFact(FactType& f) = 0;
  };

  template <class FactType>
  void CFGFlowAnalysis<FactType>::doAnalysis(Module& M, SandboxVector& sandboxes) {
    QueueSet<BasicBlock*> worklist;
    initialise(worklist, M, sandboxes);
    performDataFlowAnalysis(worklist, sandboxes, M);
    postDataFlowAnalysis(M, sandboxes);
  }

  template <typename FactType>
  void CFGFlowAnalysis<FactType>::performDataFlowAnalysis(QueueSet<BasicBlock*>& worklist, SandboxVector& sandboxes, Module& M) {
    while (!worklist.empty()) {
      BasicBlock* BB = worklist.dequeue();

      SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_3 << "BB: " << *BB << "\n");
      SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_4 << "Computing entry\n");

      // First, calculate join of predecessor blocks
      FactType entryBB = bottomValue();
      for (pred_iterator PI = pred_begin(BB), PE = pred_end(BB); PI != PE; ++PI) {
        BasicBlock* PredBB = *PI;
        TerminatorInst* T = PredBB->getTerminator();
        entryBB |= state[T];
      }

      SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_4 << "Computed entry: " << stringifyFact(entryBB) << "\n");
      SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_4 << "Propagating through current BB (" << BB->getParent()->getName() << ")\n");

      // Second, process the current basic block
      TerminatorInst* T = BB->getTerminator();
      FactType oldTerminatorState = state[T];
      Instruction* predI = NULL;
      for (Instruction& II : *BB) {
        Instruction* I = &II;
        SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_5 << "Instruction: " << *I << "\n");
        SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_6 << "State (before): " << stringifyFact(state[I]) << "\n")
        state[I] |= (predI == NULL ? entryBB : state[predI]);
        SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_6 << "State (after): " << stringifyFact(state[I]) << "\n");

        if (CallInst* CI = dyn_cast<CallInst>(I)) {
          if (!isa<IntrinsicInst>(CI)) {
            SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_6 << "Call to non-intrinsic\n");
            FunctionSet callees = CallGraphUtils::getCallees(CI, NULL, M);
            for (Function* callee : callees) {
              if (callee->isDeclaration()) continue;
              if (!SandboxUtils::isSandboxEntryPoint(M, callee) && !callee->isDeclaration()) {
                SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_6 << "Propagating to callee " << callee->getName() << "\n");
                // propagate to the entry block, and propagate back from the return blocks
                BasicBlock& calleeEntryBB = callee->getEntryBlock();
                Instruction& calleeFirstI = *calleeEntryBB.begin();
                updateStateAndPropagate(&calleeFirstI, state[I], worklist);
                SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_6 << "New state: " << stringifyFact(state[&calleeFirstI]) << "\n");
              }
            }
          }
        }
        else if (ReturnInst* RI = dyn_cast<ReturnInst>(I)) {
          // propagate to callers
          SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_6 << "Return\n");
          Function* callee = RI->getParent()->getParent();
          CallInstSet callers = CallGraphUtils::getCallers(callee, NULL, M);
          for (CallInst* CI : callers) {
            SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_6 << "Propagating to caller " << *CI << "\n");
            updateStateAndPropagate(CI, state[RI], worklist);
            SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_6 << "New state: " << stringifyFact(state[CI]) << "\n");
          }
        }
        predI = I;
        SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << "\n");
      }

      SDEBUG("soaap.analysis.cfgflow", 3, dbgs() << INDENT_4 << "Propagating to successor BBs\n");

      // Thirdly, propagate to successor blocks (if terminator's state changed)
      if (state[T] != oldTerminatorState) {
        for (succ_iterator SI = succ_begin(BB), SE = succ_end(BB); SI != SE; ++SI) {
          BasicBlock* SuccBB = *SI;
          worklist.enqueue(SuccBB); 
        }
      }

      //dbgs() << "Worklist size: " << worklist.size() << "\n";
    }
  }

  template <typename FactType>
  void CFGFlowAnalysis<FactType>::updateStateAndPropagate(Instruction* I, FactType val, QueueSet<BasicBlock*>& worklist) {
    FactType oldState = state[I];
    state[I] |= val;
    if (state[I] != oldState) {
      BasicBlock* BB = I->getParent();
      worklist.enqueue(BB);
    }
  }

}

#endif
