#include "ADT/QueueSet.h"
#include "Common/CmdLineOpts.h"
#include "Common/Debug.h"
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
      ppInstruction(*I);
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
      ppInstruction(*I);
    }
  }
  else {
    SDEBUG("soaap.pp", 3, dbgs() << "not summarising. pretty printing each instruction.");
    for (Instruction* I : trace) {
      ppInstruction(I);
    }
  }
}

void PrettyPrinters::ppInstruction(Instruction* I) {
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
