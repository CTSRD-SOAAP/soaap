#include "Analysis/GlobalVariableAnalysis.h"

#include "soaap.h"
#include "Util/CallGraphUtils.h"
#include "Util/DebugUtils.h"
#include "Util/SandboxUtils.h"
#include "Common/Sandbox.h"
#include "llvm/DebugInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>

using namespace soaap;

void GlobalVariableAnalysis::doAnalysis(Module& M, SandboxVector& sandboxes) {
  // reverse map of shared global var to sandbox for later
  map<GlobalVariable*,SandboxVector> varToSandboxes;
  
  // find all uses of global variables and check that they are allowed
  // as per the annotations
  for (Sandbox* S : sandboxes) {
    GlobalVariableIntMap varToPerms = S->getGlobalVarPerms();
    // update reverse map of global vars -> sandbox names for later
    for (GlobalVariableIntMap::iterator I=varToPerms.begin(), E=varToPerms.end(); I != E; I++) {
      varToSandboxes[I->first].push_back(S);
    }
    for (Function* F : S->getFunctions()) {
      SmallVector<GlobalVariable*,10> alreadyReportedReads, alreadyReportedWrites;
      DEBUG(dbgs() << "   Sandbox-reachable function: " << F->getName().str() << "\n");
      for (BasicBlock& BB : F->getBasicBlockList()) {
        for (Instruction& I : BB.getInstList()) {
  //            I.dump();
          if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
            if (GlobalVariable* gv = dyn_cast<GlobalVariable>(load->getPointerOperand())) {
              //outs() << "VAR_READ_MASK?: " << (varToPerms[gv] & VAR_READ_MASK) << ", sandbox-check: " << stringifySandboxNames(globalVarToSandboxNames[gv] & sandboxedMethodToNames[F]) << "\n";
              //if (gv->isDeclaration()) continue; // not concerned with externs
              if (!(varToPerms[gv] & VAR_READ_MASK)) {
                if (find(alreadyReportedReads.begin(), alreadyReportedReads.end(), gv) == alreadyReportedReads.end()) {
                  outs() << " *** Sandboxed method \"" << F->getName().str() << "\" [" << S->getName() << "] read global variable \"" << gv->getName().str() << "\" " << findGlobalDeclaration(M, gv) << "but is not allowed to. If the access is intended, the variable needs to be annotated with __soaap_read_var.\n";
                  if (MDNode *N = I.getMetadata("dbg")) {
                    DILocation loc(N);
                    outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                  }
                  alreadyReportedReads.push_back(gv);
                  outs() << "\n";
                }
              }
            }
          }
          else if (StoreInst* store = dyn_cast<StoreInst>(&I)) {
            if (GlobalVariable* gv = dyn_cast<GlobalVariable>(store->getPointerOperand())) {
              if (gv->isDeclaration()) continue; // not concerned with externs
              // check that the programmer has annotated that this
              // variable can be written to
              if (!(varToPerms[gv] & VAR_WRITE_MASK)) {
                if (find(alreadyReportedWrites.begin(), alreadyReportedWrites.end(), gv) == alreadyReportedWrites.end()) {
                  outs() << " *** Sandboxed method \"" << F->getName().str() << "\" [" << S->getName() << "] wrote to global variable \"" << gv->getName().str() << "\" " << findGlobalDeclaration(M, gv) << "but is not allowed to\n";
                  if (MDNode *N = I.getMetadata("dbg")) {
                    DILocation loc(N);
                    outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                  }
                  alreadyReportedWrites.push_back(gv);
                  outs() << "\n";
                }
              }
            }
          }
  //            I.dump();
  //            cout << "Num operands: " << I.getNumOperands() << endl;
  //            for (int i=0; i<I.getNumOperands(); i++) {
  //              cout << "Operand " << i << ": " << endl;
  //              I.getOperand(i)->dump();
  //            }
        }
      }
    }
  }

  // Look for writes to shared global variables in privileged methods
  // that are performed after a sandbox is created and thus will not 
  // be seen by the sandbox.
  checkSharedGlobalWrites(M, sandboxes, varToSandboxes);

  /*
  for (Function* F : privilegedMethods) {
    DEBUG(dbgs() << INDENT_1 << "Privileged function: " << F->getName().str() << "\n");
    SmallVector<GlobalVariable*,10> alreadyReported;
    for (BasicBlock& BB : F->getBasicBlockList()) {
      for (Instruction& I : BB.getInstList()) {
        if (StoreInst* store = dyn_cast<StoreInst>(&I)) {
          if (GlobalVariable* gv = dyn_cast<GlobalVariable>(store->getPointerOperand())) {
            // check that the programmer has annotated that this
            // variable can be read from 
            SandboxVector& varSandboxes = varToSandboxes[gv];
            int readerSandboxNames = 0;
            for (Sandbox* S : varSandboxes) {
              if (S->isAllowedToReadGlobalVar(gv)) {
                readerSandboxNames |= (1 << S->getNameIdx());
              }
            }
            if (readerSandboxNames) {
              // check that this store is preceded by a sandbox_create annotation
              DEBUG(dbgs() << "   Checking write to annotated variable " << gv->getName() << "\n");
              DEBUG(dbgs() << "   readerSandboxNames: " << SandboxUtils::stringifySandboxNames(readerSandboxNames) << "\n");
              if (find(alreadyReported.begin(), alreadyReported.end(), gv) == alreadyReported.end()) {
                outs() << " *** Write to shared variable \"" << gv->getName() << "\" outside sandbox in method \"" << F->getName() << "\" will not be seen by the sandboxes: " << SandboxUtils::stringifySandboxNames(readerSandboxNames) << ". Synchronisation is needed to to propagate this update to the sandbox.\n";
                if (MDNode *N = I.getMetadata("dbg")) {
                  DILocation loc(N);
                  outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                }
                alreadyReported.push_back(gv);
                outs() << "\n";
              }
            }
          }
        }
      }
    }
  }
  */
}

string GlobalVariableAnalysis::findGlobalDeclaration(Module& M, GlobalVariable* G) {
  NamedMDNode *NMD = M.getNamedMetadata("llvm.dbg.cu");
  ostringstream ss;
  for (int i=0; i<NMD->getNumOperands(); i++) {
    DICompileUnit CU(NMD->getOperand(i));
    DIArray globals = CU.getGlobalVariables();
    for (int j=0; j<globals.getNumElements(); j++) {
      DIGlobalVariable GV(globals.getElement(j));
      if (GV.getGlobal() == G) {
        ss << "(" << GV.getFilename().str() << ":" << GV.getLineNumber() << ") ";
        return ss.str();
      }
    }
  }
  return "";
}

// Check for writes that occur in the privileged parent after sandboxes have
// been forked. A sandbox is considered to be forked at the point where a 
// __soaap_create_persistent_sandbox annotation occurs.
void GlobalVariableAnalysis::checkSharedGlobalWrites(Module& M, SandboxVector& sandboxes, map<GlobalVariable*,SandboxVector>& varToSandboxes) {
  // At the point of a shared global variable write in the privileged parent, we
  // essentially need to see which __create annotations reach the write
  // instruction. 
  
  // Thus, the first thing to do is calculate the reaching __create annotations
  // at each program point. We do not need to propagate into a sandbox
  // entrypoint function. The existing taint analysis framework follows use-def
  // chains where here we follow control flow.

  DEBUG(dbgs() << INDENT_1 << "Calculating reaching sandbox creations\n");

  // Analysis state and worklist
  map<Instruction*,int> reachingCreationsEntry;
  map<Instruction*,int> reachingCreationsExit;
  list<BasicBlock*> worklist;

  DEBUG(dbgs() << INDENT_2 << "Initialising worklist with BBs containing creation annotations\n");

  // Initialise worklist with basic blocks that contain creation points.
  for (Sandbox* S : sandboxes) {
    CallInstVector CV = S->getCreationPoints();
    for (CallInst* C : CV) {
      reachingCreationsExit[C] = (1 << S->getNameIdx()); // each creation point creates one sandbox
      DEBUG(dbgs() << INDENT_3 << "Added BB for creation point " << *C << "\n");
      BasicBlock* BB = C->getParent();
      if (find(worklist.begin(), worklist.end(), BB) == worklist.end()) {
        worklist.push_back(BB);
      }
    }
  }

  DEBUG(dbgs() << INDENT_2 << "Worklist contains " << worklist.size() << " BBs\n");
  DEBUG(dbgs() << INDENT_2 << "Computing fixed point\n");

  while (!worklist.empty()) {
    BasicBlock* BB = worklist.front();
    worklist.pop_front();

    DEBUG(dbgs() << INDENT_3 << "BB: " << *BB << "\n");
    DEBUG(dbgs() << INDENT_4 << "Computing entry\n");

    // First, calculate join of predecessor blocks
    int reachingCreationsPredBB = 0;
    for (pred_iterator PI = pred_begin(BB), PE = pred_end(BB); PI != PE; ++PI) {
      BasicBlock* PredBB = *PI;
      TerminatorInst* T = PredBB->getTerminator();
      reachingCreationsPredBB |= reachingCreationsExit[T];
    }

    DEBUG(dbgs() << INDENT_4 << "Computed entry: " << SandboxUtils::stringifySandboxNames(reachingCreationsPredBB) << "\n");
    DEBUG(dbgs() << INDENT_4 << "Propagating through current BB (" << BB->getParent()->getName() << ")\n");

    // Second, process the current basic block
    TerminatorInst* T = BB->getTerminator();
    int oldTerminatorExit = reachingCreationsExit[T];
    Instruction* predI;
    for (Instruction& II : *BB) {
      Instruction* I = &II;
      DEBUG(dbgs() << INDENT_5 << "Instruction: " << *I << "\n");
      reachingCreationsEntry[I] |= (predI == NULL ? reachingCreationsPredBB : reachingCreationsExit[predI]);
      reachingCreationsExit[I] |= reachingCreationsEntry[I];
      DEBUG(dbgs() << INDENT_6 << "Entry: " << SandboxUtils::stringifySandboxNames(reachingCreationsEntry[I]) << "\n");
      DEBUG(dbgs() << INDENT_6 << "Exit: " << SandboxUtils::stringifySandboxNames(reachingCreationsExit[I]) << "\n");

      if (CallInst* CI = dyn_cast<CallInst>(I)) {
        if (!isa<IntrinsicInst>(CI)) {
          DEBUG(dbgs() << INDENT_6 << "Call to non-intrinsic\n");
          FunctionVector callees = CallGraphUtils::getCallees(CI, M);
          for (Function* callee : callees) {
            if (!SandboxUtils::isSandboxEntryPoint(M, callee) && !callee->isDeclaration()) {
              DEBUG(dbgs() << INDENT_6 << "Propagating to callee " << callee->getName() << "\n");
              // propagate to the entry block, and propagate back from the return blocks
              BasicBlock& calleeEntryBB = callee->getEntryBlock();
              Instruction& calleeFirstI = *calleeEntryBB.begin();
              updateReachingCreationsStateAndPropagate(reachingCreationsEntry, &calleeFirstI, reachingCreationsEntry[I], worklist);
              DEBUG(dbgs() << INDENT_6 << "New Entry: " << SandboxUtils::stringifySandboxNames(reachingCreationsEntry[&calleeFirstI]) << "\n");

            }
          }
        }
      }
      else if (ReturnInst* RI = dyn_cast<ReturnInst>(I)) {
        // propagate to callers
        DEBUG(dbgs() << INDENT_6 << "Return\n");
        Function* callee = RI->getParent()->getParent();
        CallInstVector callers = CallGraphUtils::getCallers(callee, M);
        for (CallInst* CI : callers) {
          DEBUG(dbgs() << INDENT_6 << "Propagating to caller " << *CI << "\n");
          updateReachingCreationsStateAndPropagate(reachingCreationsExit, CI, reachingCreationsEntry[RI], worklist);
          DEBUG(dbgs() << INDENT_6 << "New Exit: " << SandboxUtils::stringifySandboxNames(reachingCreationsExit[CI]) << "\n");
        }
      }
      predI = I;
      DEBUG(dbgs() << "\n");
    }

    DEBUG(dbgs() << INDENT_4 << "Propagating to successor BBs\n");

    // Thirdly, propagate to successor blocks (if terminator's state changed)
    if (reachingCreationsExit[T] != oldTerminatorExit) {
      for (succ_iterator SI = succ_begin(BB), SE = succ_end(BB); SI != SE; ++SI) {
        BasicBlock* SuccBB = *SI;
        if (find(worklist.begin(), worklist.end(), SuccBB) == worklist.end()) {
          worklist.push_back(SuccBB);
        }
      }
    }
  }

  // Now check for each privileged write, whether it may be preceded by a sandbox-creation
  // annotation.
  for (Function* F : privilegedMethods) {
    DEBUG(dbgs() << INDENT_1 << "Privileged function: " << F->getName().str() << "\n");
    SmallVector<GlobalVariable*,10> alreadyReported;
    for (BasicBlock& BB : F->getBasicBlockList()) {
      for (Instruction& I : BB.getInstList()) {
        if (StoreInst* store = dyn_cast<StoreInst>(&I)) {
          if (GlobalVariable* gv = dyn_cast<GlobalVariable>(store->getPointerOperand())) {
            // check that the programmer has annotated that this
            // variable can be read from 
            SandboxVector& varSandboxes = varToSandboxes[gv];
            int readerSandboxNames = 0;
            for (Sandbox* S : varSandboxes) {
              if (S->isAllowedToReadGlobalVar(gv)) {
                readerSandboxNames |= (1 << S->getNameIdx());
              }
            }
            int possInconsSandboxes = readerSandboxNames & reachingCreationsEntry[store];
            if (possInconsSandboxes) {
              // check that this store is preceded by a sandbox_create annotation
              DEBUG(dbgs() << "   Checking write to annotated variable " << gv->getName() << "\n");
              DEBUG(dbgs() << "   readerSandboxNames: " << SandboxUtils::stringifySandboxNames(readerSandboxNames) << ", reachingCreationsEntry: " << SandboxUtils::stringifySandboxNames(reachingCreationsEntry[store]) << ", possInconsSandboxes: " << SandboxUtils::stringifySandboxNames(possInconsSandboxes) << "\n");
              if (find(alreadyReported.begin(), alreadyReported.end(), gv) == alreadyReported.end()) {
                outs() << " *** Write to shared variable \"" << gv->getName() << "\" " << findGlobalDeclaration(M, gv) << "outside sandbox in method \"" << F->getName() << "\" will not be seen by the sandboxes: " << SandboxUtils::stringifySandboxNames(possInconsSandboxes) << ". Synchronisation is needed to to propagate this update to the sandbox.\n";
                if (MDNode *N = I.getMetadata("dbg")) {
                  DILocation loc(N);
                  outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                }
                alreadyReported.push_back(gv);
                outs() << "\n";
              }
            }
          }
        }
      }
    }
  }
}

void GlobalVariableAnalysis::updateReachingCreationsStateAndPropagate(map<Instruction*,int>& state, Instruction* I, int val, list<BasicBlock*>& worklist) {
  int oldState = state[I];
  state[I] |= val;
  if (state[I] != oldState) {
    BasicBlock* BB = I->getParent();
    if (find(worklist.begin(), worklist.end(), BB) == worklist.end()) {
      worklist.push_back(BB);
    }
  }
}
