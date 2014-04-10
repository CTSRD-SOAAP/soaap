#include "Analysis/CFGFlow/GlobalVariableAnalysis.h"

#include "soaap.h"
#include "Common/CmdLineOpts.h"
#include "Common/Debug.h"
#include "Common/Sandbox.h"
#include "Util/CallGraphUtils.h"
#include "Util/DebugUtils.h"
#include "Util/SandboxUtils.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>

using namespace soaap;

void GlobalVariableAnalysis::initialise(QueueSet<BasicBlock*>& worklist, Module& M, SandboxVector& sandboxes) {
  // Initialise worklist with basic blocks that contain creation points.
  for (Sandbox* S : sandboxes) {
    CallInstVector CV = S->getCreationPoints();
    SDEBUG("soaap.analysis.globals", 3, dbgs() << "Total number of sandboxed functions: " << S->getFunctions().size() << "\n");
    for (CallInst* C : CV) {
      state[C] = (1 << S->getNameIdx()); // each creation point creates one sandbox
      SDEBUG("soaap.analysis.globals", 3, dbgs() << INDENT_3 << "Added BB for creation point " << *C << "\n");
      BasicBlock* BB = C->getParent();
      worklist.enqueue(BB);
    }
  }
}

void GlobalVariableAnalysis::postDataFlowAnalysis(Module& M, SandboxVector& sandboxes) {
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
      SDEBUG("soaap.analysis.globals", 3, dbgs() << "   Sandbox-reachable function: " << F->getName().str() << "\n");
      for (BasicBlock& BB : F->getBasicBlockList()) {
        for (Instruction& I : BB.getInstList()) {
  //            I.dump();
          if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
            Value* operand = load->getPointerOperand()->stripPointerCasts();
            if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(operand)) {
              operand = gep->getPointerOperand();
            }
            if (GlobalVariable* gv = dyn_cast<GlobalVariable>(operand)) {
              //outs() << "VAR_READ_MASK?: " << (varToPerms[gv] & VAR_READ_MASK) << ", sandbox-check: " << stringifySandboxNames(globalVarToSandboxNames[gv] & sandboxedMethodToNames[F]) << "\n";
              //if (gv->isDeclaration()) continue; // not concerned with externs
              if (!(varToPerms[gv] & VAR_READ_MASK)) {
                if (CmdLineOpts::Pedantic || find(alreadyReportedReads.begin(), alreadyReportedReads.end(), gv) == alreadyReportedReads.end()) {
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
            Value* operand = store->getPointerOperand()->stripPointerCasts();
            if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(operand)) {
              operand = gep->getPointerOperand();
            }
            if (GlobalVariable* gv = dyn_cast<GlobalVariable>(operand)) {
              if (gv->isDeclaration()) continue; // not concerned with externs
              // check that the programmer has annotated that this
              // variable can be written to
              if (!(varToPerms[gv] & VAR_WRITE_MASK)) {
                if (CmdLineOpts::Pedantic || find(alreadyReportedWrites.begin(), alreadyReportedWrites.end(), gv) == alreadyReportedWrites.end()) {
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

  /*
  for (Function* F : privilegedMethods) {
    SDEBUG(dbgs() << INDENT_1 << "Privileged function: " << F->getName().str() << "\n");
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
              SDEBUG(dbgs() << "   Checking write to annotated variable " << gv->getName() << "\n");
              SDEBUG(dbgs() << "   readerSandboxNames: " << SandboxUtils::stringifySandboxNames(readerSandboxNames) << "\n");
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

  // Now check for each privileged write, whether it may be preceded by a sandbox-creation
  // annotation.
  for (Function* F : privilegedMethods) {
    SDEBUG("soaap.analysis.globals", 3, dbgs() << INDENT_1 << "Privileged function: " << F->getName().str() << "\n");
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
            int possInconsSandboxes = readerSandboxNames & state[store];
            if (possInconsSandboxes) {
              // check that this store is preceded by a sandbox_create annotation
              SDEBUG("soaap.analysis.globals", 3, dbgs() << "   Checking write to annotated variable " << gv->getName() << "\n");
              SDEBUG("soaap.analysis.globals", 3, dbgs() << "   readerSandboxNames: " << SandboxUtils::stringifySandboxNames(readerSandboxNames) << ", reaching creations: " << SandboxUtils::stringifySandboxNames(state[store]) << ", possInconsSandboxes: " << SandboxUtils::stringifySandboxNames(possInconsSandboxes) << "\n");
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

string GlobalVariableAnalysis::findGlobalDeclaration(Module& M, GlobalVariable* G) {
  if (NamedMDNode *NMD = M.getNamedMetadata("llvm.dbg.cu")) {
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
  }
  return "";
}
