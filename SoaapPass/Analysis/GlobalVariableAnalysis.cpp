#include "Analysis/GlobalVariableAnalysis.h"

#include "soaap.h"
#include "Util/DebugUtils.h"
#include "Util/SandboxUtils.h"
#include "Common/Sandbox.h"
#include "llvm/DebugInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

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
              //if (gv->hasExternalLinkage()) continue; // not concerned with externs
              if (!(varToPerms[gv] & VAR_READ_MASK)) {
                if (find(alreadyReportedReads.begin(), alreadyReportedReads.end(), gv) == alreadyReportedReads.end()) {
                  outs() << " *** Sandboxed method \"" << F->getName().str() << "\" read global variable \"" << gv->getName().str() << "\" but is not allowed to. If the access is intended, the variable needs to be annotated with __soaap_read_var.\n";
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
              if (gv->hasExternalLinkage()) continue; // not concerned with externs
              // check that the programmer has annotated that this
              // variable can be written to
              if (!(varToPerms[gv] & VAR_WRITE_MASK)) {
                if (find(alreadyReportedWrites.begin(), alreadyReportedWrites.end(), gv) == alreadyReportedWrites.end()) {
                  outs() << " *** Sandboxed method \"" << F->getName().str() << "\" wrote to global variable \"" << gv->getName().str() << "\" but is not allowed to\n";
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
  // that will therefore not be seen by sandboxes (assuming that the
  // the sandbox process is forked at the start of main).
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
}
