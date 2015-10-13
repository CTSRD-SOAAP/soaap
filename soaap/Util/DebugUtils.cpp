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

#include "Util/DebugUtils.h"

#include "Common/Debug.h"

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;

map<Function*, string> DebugUtils::funcToLib;
bool DebugUtils::cachingDone = false;

void DebugUtils::cacheLibraryMetadata(Module* M) {
  if (NamedMDNode* N = M->getNamedMetadata("llvm.libs")) {
    SDEBUG("soaap.util.debug", 3, dbgs() << "Found llvm.libs metadata, " << N->getNumOperands() << " operands:\n");
    for (int i=0; i<N->getNumOperands(); i++) {
      MDNode* lib = N->getOperand(i);
      MDString* name = cast<MDString>(lib->getOperand(0).get());
      string nameStr = name->getString().str();
      SDEBUG("soaap.util.debug", 3, dbgs() << "Processing lib " << nameStr << "\n");
      MDTuple* cus = cast<MDTuple>(lib->getOperand(1).get());
      for (int j=0; j<cus->getNumOperands(); j++) {
        DICompileUnit* cu = cast<DICompileUnit>(cus->getOperand(j).get());
        DISubprogramArray funcs = cu->getSubprograms();
        for (int k=0; k<funcs.size(); k++) {
          DISubprogram* func = funcs[k];
          if (Function* F = func->getFunction()) {
            SDEBUG("soaap.util.debug", 4, dbgs() << INDENT_1 << "Found func: " << F->getName() << "\n");
            if (funcToLib.find(F) != funcToLib.end()) {
              SDEBUG("soaap.util.debug", 3, dbgs() << "WARNING: Function "
                                                   << F->getName()
                                                   << " already exists in library "
                                                   << funcToLib[F] << "\n");
            }
            else {
              funcToLib[F] = nameStr;
            }
          }
          else {
            SDEBUG("soaap.util.debug", 3, dbgs() << "DISubprogram \"" << func->getName() << "\" has no Function*\n");
          }
        }
      }
    }
  }
  cachingDone = true;
}

string DebugUtils::getEnclosingLibrary(Instruction* I) {
  return getEnclosingLibrary(I->getParent()->getParent());
}

string DebugUtils::getEnclosingLibrary(Function* F) {
  SDEBUG("soaap.util.debug", 3, dbgs() << "Finding enclosing library for inst in func " << F->getName() << "\n");
  if (!cachingDone) {
    cacheLibraryMetadata(F->getParent());
  }
  if (funcToLib.find(F) == funcToLib.end()) {
    SDEBUG("soaap.util.debug", 3, dbgs() << "Didn't find library for function " << F->getName() << "\n");
    return "";
  }
  else {
    return funcToLib[F];
  }
}

pair<string,int> DebugUtils::findGlobalDeclaration(GlobalVariable* G) {
  Module* M = G->getParent();
  if (NamedMDNode *NMD = M->getNamedMetadata("llvm.dbg.cu")) {
    for (int i=0; i<NMD->getNumOperands(); i++) {
      DICompileUnit* CU = cast<DICompileUnit>(NMD->getOperand(i));
      DIGlobalVariableArray globals = CU->getGlobalVariables();
      for (int j=0; j<globals.size(); j++) {
        DIGlobalVariable* GV = globals[j];
        if (GV->getVariable() == G) {
          return make_pair<string,int>(GV->getFilename().str(), GV->getLine());
        }
      }
    }
  }
  return make_pair<string,int>("",-1); 
}

tuple<string,int,string> DebugUtils::getInstLocation(Instruction* I) {
  if (DILocation* loc = dyn_cast_or_null<DILocation>(I->getMetadata("dbg"))) {
    Function* enclosingFunc = I->getParent()->getParent();
    string library = getEnclosingLibrary(enclosingFunc);
    return make_tuple(loc->getFilename(), loc->getLine(), library);
  }
  return make_tuple("",-1,"");
}

