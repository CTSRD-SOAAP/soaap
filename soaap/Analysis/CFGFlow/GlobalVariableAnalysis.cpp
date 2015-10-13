/*
 * Copyright (c) 2013-2015 Khilan Gudka
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "Analysis/CFGFlow/GlobalVariableAnalysis.h"

#include "soaap.h"
#include "Common/CmdLineOpts.h"
#include "Common/Debug.h"
#include "Common/Sandbox.h"
#include "Common/XO.h"
#include "Util/CallGraphUtils.h"
#include "Util/DebugUtils.h"
#include "Util/SandboxUtils.h"
#include "Util/PrettyPrinters.h"
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
  XO::List globalAccessWarningList("global_access_warning");
  for (Sandbox* S : sandboxes) {
    GlobalVariableIntMap varToPerms = S->getGlobalVarPerms();
    // update reverse map of global vars -> sandbox names for later
    for (GlobalVariableIntMap::iterator I=varToPerms.begin(), E=varToPerms.end(); I != E; I++) {
      varToSandboxes[I->first].push_back(S);
    }
    for (Function* F : S->getFunctions()) {
      if (shouldOutputWarningFor(F)) {
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
                    pair<string,int> declareLoc = DebugUtils::findGlobalDeclaration(gv);
                    string declareLocStr = "";
                    if (declareLoc.second != -1) {
                      stringstream ss;
                      ss << "(" << declareLoc.first << ":" << declareLoc.second << ")";
                      declareLocStr = ss.str();
                    }
                    XO::Instance globalAccessWarningInstance(globalAccessWarningList);
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
                      XO::Container declareLocContainer("declare_loc");
                      XO::emit("{e:line/%d}{e:file/%s}",
                               declareLoc.second, declareLoc.first.c_str());
                    }
                    PrettyPrinters::ppInstruction(&I);
                    if (CmdLineOpts::isSelected(SoaapAnalysis::Globals, CmdLineOpts::OutputTraces)) {
                      CallGraphUtils::emitCallTrace(F, S, M);
                    }
                    alreadyReportedReads.push_back(gv);
                    XO::emit("\n");
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
                    pair<string,int> declareLoc = DebugUtils::findGlobalDeclaration(gv);
                    string declareLocStr = "";
                    if (declareLoc.second != -1) {
                      stringstream ss;
                      ss << "(" << declareLoc.first << ":" << declareLoc.second << ")";
                      declareLocStr = ss.str();
                    }
                    XO::Instance globalAccessWarningInstance(globalAccessWarningList);
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
                      XO::Container declareLocContainer("declare_loc");
                      XO::emit("{e:line/%d}{e:file%s}",
                               declareLoc.second, declareLoc.first.c_str());
                    }
                    PrettyPrinters::ppInstruction(&I);
                    if (CmdLineOpts::isSelected(SoaapAnalysis::Globals, CmdLineOpts::OutputTraces)) {
                      CallGraphUtils::emitCallTrace(F, S, M);
                    }
                    alreadyReportedWrites.push_back(gv);
                    XO::emit("\n");
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  globalAccessWarningList.close();

  // Now check for each privileged write, whether it may be preceded by a sandbox-creation
  // annotation.
  XO::List globalLostUpdateList("global_lost_update");
  for (Function* F : privilegedMethods) {
    if (shouldOutputWarningFor(F)) {
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
                  pair<string,int> declareLoc = DebugUtils::findGlobalDeclaration(gv);
                  string declareLocStr = "";
                  if (declareLoc.second != -1) {
                    stringstream ss;
                    ss << "(" << declareLoc.first << ":" << declareLoc.second << ")";
                    declareLocStr = ss.str();
                  }
                  XO::Instance globalLostUpdateInstance(globalLostUpdateList);
                  XO::emit(" *** Write to shared variable \"{:var_name/%s}\" "
                           "{d:declare_loc/%s} outside sandbox in method "
                           "\"{:function/%s}\" will not be seen by the sandboxes: "
                           "{d:sandboxes/%s}. Synchronisation is needed to "
                           "propagate this update to the sandboxes.\n",
                           gv->getName().str().c_str(),
                           declareLocStr.c_str(),
                           F->getName().str().c_str(),
                           SandboxUtils::stringifySandboxNames(possInconsSandboxes).c_str());
                  XO::List sandboxList("sandbox");
                  for (Sandbox* S : SandboxUtils::convertNamesToVector(possInconsSandboxes, sandboxes)) {
                    XO::Instance sandboxInstance(sandboxList);
                    XO::emit("{e:name/%s}", S->getName().c_str());
                  }
                  sandboxList.close();
                  if (declareLoc.second != -1) {
                    XO::Container declareLocContainer("declare_loc");
                    XO::emit("{e:line/%d}{e:file/%s}",
                             declareLoc.second, declareLoc.first.c_str());
                  }
                  PrettyPrinters::ppInstruction(&I);
                  alreadyReported.push_back(gv);
                  XO::emit("\n");
                }
              }
            }
          }
        }
      }
    }
  }
  globalLostUpdateList.close();
}
