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

#include "ADT/QueueSet.h"
#include "Common/CmdLineOpts.h"
#include "Common/Debug.h"
#include "Common/XO.h"
#include "Util/CallGraphUtils.h"
#include "Util/PrettyPrinters.h"
#include "Util/LLVMAnalyses.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;

void PrettyPrinters::ppPrivilegedPathToFunction(Function* Target, Module& M) {
  SDEBUG("soaap.pp", 3, dbgs() << "printing privileged path to function \"" << Target->getName() << "\"\n");
  InstTrace stack = CallGraphUtils::findPrivilegedPathToFunction(Target, M);
  ppTrace(stack);
}

void PrettyPrinters::ppTaintSource(CallInst* C) {
  outs() << "    Source of untrusted data:\n";
  Function* EnclosingFunc = cast<Function>(C->getParent()->getParent());
  if (DILocation* Loc = dyn_cast_or_null<DILocation>(C->getMetadata("dbg"))) {
    unsigned Line = Loc->getLine();
    StringRef File = Loc->getFilename();
    unsigned FileOnlyIdx = File.find_last_of("/");
    StringRef FileOnly = FileOnlyIdx == -1 ? File : File.substr(FileOnlyIdx+1);
    outs() << "      " << EnclosingFunc->getName() << "(" << FileOnly << ":" << Line << ")\n";
  }
}

void PrettyPrinters::ppTrace(InstTrace& trace) {
  bool summariseTrace = CmdLineOpts::SummariseTraces != 0 && (CmdLineOpts::SummariseTraces*2 < trace.size());
  if (summariseTrace) {
    InstTrace::iterator I=trace.begin();
    int i=0;
    for (; i<CmdLineOpts::SummariseTraces; I++, i++) {
      ppInstructionForTrace(*I);
    }
    outs() << "      ...\n";
    outs() << "      ...\n";
    outs() << "      ...\n";
    // fast forward to the end
    while (trace.size()-i > CmdLineOpts::SummariseTraces) {
      i++;
      I++;
    }
    for (; i<trace.size(); I++, i++) {
      ppInstructionForTrace(*I);
    }
  }
  else {
    SDEBUG("soaap.pp", 3, dbgs() << "not summarising. pretty printing each instruction.");
    for (Instruction* I : trace) {
      ppInstructionForTrace(I);
    }
  }
}

void PrettyPrinters::ppInstructionForTrace(Instruction* I) {
  if (DILocation* Loc = dyn_cast_or_null<DILocation>(I->getMetadata("dbg"))) {
    Function* EnclosingFunc = cast<Function>(I->getParent()->getParent());
    unsigned Line = Loc->getLine();
    StringRef File = Loc->getFilename();
    unsigned FileOnlyIdx = File.find_last_of("/");
    StringRef FileOnly = FileOnlyIdx == -1 ? File : File.substr(FileOnlyIdx+1);
    outs() << "      " << EnclosingFunc->getName() << "(" << FileOnly << ":" << Line << ")\n";
  }
  else {
    errs() << "Warning: instruction does not contain debug metadata\n";
  }
}

void PrettyPrinters::ppInstruction(Instruction* I, bool displayText) {
  if (DILocation* loc = dyn_cast_or_null<DILocation>(I->getMetadata("dbg"))) {
    XO::Container locationContainer("location");
    if (displayText) {
      XO::emit(
        " +++ Line {d:line/%d} of file {d:file/%s}",
        loc->getLine(),
        loc->getFilename().str().c_str());
    }
    XO::emit(
      "{e:line/%d}{e:file/%s}",
      loc->getLine(),
      loc->getFilename().str().c_str());
    string library = DebugUtils::getEnclosingLibrary(I);
    if (!library.empty()) {
      if (displayText) {
        XO::emit(" ({d:library/%s} library)", library.c_str());
      }
      XO::emit("{e:library/%s}", library.c_str());
    }
    if (displayText) {
      XO::emit("\n");
    }
  }
}
