#include "Analysis/CFGFlow/GlobalVariableAnalysis.h"

#include "soaap.h"
#include "Common/CmdLineOpts.h"
#include "Common/Debug.h"
#include "Common/Sandbox.h"
#include "Common/XO.h"
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
  XO::open_list("global_access_warning");
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
                  SDEBUG("soaap.analysis.globals", 3, dbgs() << "  Found unannotated read to global \"" << gv->getName() << "\"\n");
                  pair<string,int> declareLoc = findGlobalDeclaration(M, gv);
                  string declareLocStr = "";
                  if (declareLoc.second != -1) {
                    stringstream ss;
                    ss << "(" << declareLoc.first << ":" << declareLoc.second << ")";
                    declareLocStr = ss.str();
                  }
                  XO::open_instance("global_access_warning");
                  XO::emit(
                    " *** Sandboxed method \"{:function/%s}\" [{:sandbox/%s}] "
                    "{:access_type/%s} global variable \"{:var_name/%s}\" "
                    "{d:declare_loc/%s} but is not allowed to. If the access "
                    "is intended, the variable needs to be annotated with "
                    "__soaap_var_read.\n",
                    F->getName().str().c_str(),
                    S->getName().c_str(),
                    "read",
                    gv->getName().str().c_str(),
                    declareLocStr.c_str());
                  if (declareLoc.second != -1) {
                    XO::open_container("declare_loc");
                    XO::emit("{e:line/%d}{e:file/%s}",
                             declareLoc.second, declareLoc.first.c_str());
                    XO::close_container("declare_loc");
                  }
                  if (MDNode *N = I.getMetadata("dbg")) {
                    DILocation loc(N);
                    XO::emit(
                      " +++ Line {:line/%d} of file {:file/%s}\n",
                      loc.getLineNumber(),
                      loc.getFilename().str().c_str());
                  }
                  alreadyReportedReads.push_back(gv);
                  XO::emit("\n");
                  XO::close_instance("global_access_warning");
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
                  pair<string,int> declareLoc = findGlobalDeclaration(M, gv);
                  string declareLocStr = "";
                  if (declareLoc.second != -1) {
                    stringstream ss;
                    ss << "(" << declareLoc.first << ":" << declareLoc.second << ")";
                    declareLocStr = ss.str();
                  }
                  XO::open_instance("global_access_warning");
                  XO::emit(
                    " *** Sandboxed method \"{:function/%s}\" [{:sandbox/%s}] "
                    "{e:access_type/%s}wrote to global variable \"{:var_name/%s}\" "
                    "{d:declare_loc/%s} but is not allowed to. If the access "
                    "is intended, the variable needs to be annotated with "
                    "__soaap_var_write.\n",
                    F->getName().str().c_str(),
                    S->getName().c_str(),
                    "write",
                    gv->getName().str().c_str(),
                    declareLocStr.c_str());
                  if (declareLoc.second != -1) {
                    XO::open_container("declare_loc");
                    XO::emit("{e:line_number/%d}{e:filename/%s}",
                             declareLoc.second, declareLoc.first.c_str());
                    XO::close_container("declare_loc");
                  }
                  if (MDNode *N = I.getMetadata("dbg")) {
                    DILocation loc(N);
                    XO::emit(
                      " +++ Line {:line_number/%d} of file {:filename/%s}\n",
                      loc.getLineNumber(),
                      loc.getFilename().str().c_str());
                  }
                  alreadyReportedWrites.push_back(gv);
                  XO::emit("\n");
                  XO::close_instance("global_access_warning");
                }
              }
            }
          }
        }
      }
    }
  }
  XO::close_list("global_access_warning");

  // Now check for each privileged write, whether it may be preceded by a sandbox-creation
  // annotation.
  XO::open_list("global_lost_update");
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
                pair<string,int> declareLoc = findGlobalDeclaration(M, gv);
                string declareLocStr = "";
                if (declareLoc.second != -1) {
                  stringstream ss;
                  ss << "(" << declareLoc.first << ":" << declareLoc.second << ")";
                  declareLocStr = ss.str();
                }
                XO::open_instance("global_lost_update");
                XO::emit(" *** Write to shared variable \"{:var_name/%s}\" "
                         "{d:declare_loc/%s} outside sandbox in method "
                         "\"{:function/%s}\" will not be seen by the sandboxes: "
                         "{d:sandboxes/%s}. Synchronisation is needed to "
                         "propagate this update to the sandboxes.\n",
                         gv->getName().str().c_str(),
                         declareLocStr.c_str(),
                         F->getName().str().c_str(),
                         SandboxUtils::stringifySandboxNames(possInconsSandboxes).c_str());
                XO::open_list("sandbox");
                for (Sandbox* S : SandboxUtils::convertNamesToVector(possInconsSandboxes, sandboxes)) {
                  XO::open_instance("sandbox");
                  XO::emit("{e:name/%s}", S->getName().c_str());
                  XO::close_instance("sandbox");
                }
                XO::close_list("sandbox");
                if (declareLoc.second != -1) {
                  XO::open_container("declare_loc");
                  XO::emit("{e:line_number/%d}{e:filename/%s}",
                           declareLoc.second, declareLoc.first.c_str());
                  XO::close_container("declare_loc");
                }
                if (MDNode *N = I.getMetadata("dbg")) {
                  DILocation loc(N);
                  XO::emit(
                    " +++ Line {:line_number/%d} of file {:filename/%s}\n",
                    loc.getLineNumber(),
                    loc.getFilename().str().c_str());
                }
                alreadyReported.push_back(gv);
                XO::emit("\n");
                XO::close_instance("global_lost_update");
              }
            }
          }
        }
      }
    }
  }
  XO::close_list("global_lost_update");
}

pair<string,int> GlobalVariableAnalysis::findGlobalDeclaration(Module& M, GlobalVariable* G) {
  if (NamedMDNode *NMD = M.getNamedMetadata("llvm.dbg.cu")) {
    for (int i=0; i<NMD->getNumOperands(); i++) {
      DICompileUnit CU(NMD->getOperand(i));
      DIArray globals = CU.getGlobalVariables();
      for (int j=0; j<globals.getNumElements(); j++) {
        DIGlobalVariable GV(globals.getElement(j));
        if (GV.getGlobal() == G) {
          return make_pair<string,int>(GV.getFilename().str(), GV.getLineNumber());
        }
      }
    }
  }
  return make_pair<string,int>("",-1); 
}
